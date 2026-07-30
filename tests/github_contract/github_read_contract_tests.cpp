#include "github_read_contract.hpp"
#include "github_graphql_operation_policy.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github::detail;

constexpr std::string_view head_oid =
    "2222222222222222222222222222222222222222";
constexpr std::string_view base_oid =
    "1111111111111111111111111111111111111111";

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] auto read_all(const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    expect(input.good(), "fixture file opens");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

[[nodiscard]] auto parse_object(const std::filesystem::path& path)
    -> QJsonObject
{
    QJsonParseError error;
    const auto bytes = QByteArray::fromStdString(read_all(path));
    const auto document = QJsonDocument::fromJson(bytes, &error);
    expect(
        error.error == QJsonParseError::NoError && document.isObject(),
        "fixture manifest is valid JSON");
    return document.object();
}

[[nodiscard]] auto utf8(const QString& value) -> std::string
{
    return value.toUtf8().toStdString();
}

struct loaded_recording
{
    std::vector<recorded_exchange> Exchanges;
    std::vector<operation_response> Responses;
};

[[nodiscard]] auto load_recording(
    const std::string_view directory,
    const server_contract server) -> loaded_recording
{
    const auto root = std::filesystem::path{
        BPR_GITHUB_CONTRACT_FIXTURE_ROOT}
        / directory;
    const auto manifest = parse_object(root / "manifest.json");
    const auto interactions = manifest.value("interactions").toArray();
    expect(
        interactions.size()
            == static_cast<qsizetype>(read_operation_catalog.size()),
        "manifest has exactly ten interactions");

    loaded_recording result;
    result.Exchanges.reserve(read_operation_catalog.size());
    result.Responses.reserve(read_operation_catalog.size());
    for(qsizetype index = 0; index < interactions.size(); ++index)
    {
        const auto interaction = interactions[index].toObject();
        const auto parsed_operation = read_operation_from_name(
            utf8(interaction.value("operation").toString()));
        expect(parsed_operation.has_value(), "manifest operation is known");
        const auto operation = *parsed_operation;
        expect(
            operation
                == read_operation_catalog[
                    static_cast<std::size_t>(index)],
            "manifest operations use canonical order");

        std::vector<http_header> headers;
        const auto header_object =
            interaction.value("safe_headers").toObject();
        headers.reserve(static_cast<std::size_t>(header_object.size()));
        for(
            auto iterator = header_object.constBegin();
            iterator != header_object.constEnd();
            ++iterator)
        {
            headers.push_back(
                http_header{
                    .Name = utf8(iterator.key()),
                    .Value = utf8(iterator.value().toString()),
                });
        }

        const auto method = parse_recorded_http_method(
            utf8(interaction.value("method").toString()));
        expect(
            method.has_value(),
            "manifest method is exactly GET or POST");
        const auto body_value = interaction.value("body");
        const auto body =
            body_value.isNull() ? std::string{}
                                : utf8(body_value.toString());
        const auto response_path =
            root
            / utf8(interaction.value("response_file").toString());
        wire_response response{
            .Status = 200,
            .Headers =
                {
                    {
                        .Name = "Content-Type",
                        .Value = "application/json",
                    },
                },
            .Body = read_all(response_path),
            .Completeness = page_completeness::complete,
        };
        result.Exchanges.push_back(
            recorded_exchange{
                .Request =
                    {
                        .Semantic = {},
                        .Server = server,
                        .Operation = operation,
                        .Method = *method,
                        .Path =
                            utf8(interaction.value("path").toString()),
                        .Query = utf8(
                            interaction.value("canonical_query")
                                .toString()),
                        .Headers = std::move(headers),
                        .Body = body,
                    },
                .Response = response,
            });
        result.Responses.push_back(
            operation_response{
                .Operation = operation,
                .Response = std::move(response),
            });
    }
    return result;
}

[[nodiscard]] auto capabilities() -> core::github_capabilities
{
    return {
        .ReviewThreads = core::github_capability_state::supported,
        .ViewedFileMutations =
            core::github_capability_state::unsupported,
        .ResolveThread = core::github_capability_state::unsupported,
        .UnresolveThread = core::github_capability_state::unsupported,
    };
}

[[nodiscard]] auto canonical_rate_limits()
    -> application::github_rate_limit_state
{
    return {
        .Core =
            application::github_rate_limit_record{
                .Resource = application::github_rate_limit_resource::core,
                .Limit = 5'000,
                .Remaining = 4'999,
                .ResetUnixSeconds = 2'000'000'000,
            },
        .Search =
            application::github_rate_limit_record{
                .Resource = application::github_rate_limit_resource::search,
                .Limit = 30,
                .Remaining = 29,
                .ResetUnixSeconds = 2'000'000'000,
            },
        .Graphql =
            application::github_rate_limit_record{
                .Resource = application::github_rate_limit_resource::graphql,
                .Limit = 5'000,
                .Remaining = 4'999,
                .ResetUnixSeconds = 2'000'000'000,
            },
    };
}

[[nodiscard]] auto normalization_context(const server_contract server)
    -> github_read_normalization_context
{
    return {
        .Server = server,
        .SessionRequest =
            {
                .Request = core::request_id{1},
                .ApplicationSession = core::application_session_id{7},
                .Profile = core::host_profile_id{11},
                .IdentityGeneration =
                    core::github_identity_generation{3},
            },
        .PullRequestRequest =
            {
                .Request = core::request_id{2},
                .ApplicationSession = core::application_session_id{7},
                .Profile = core::host_profile_id{11},
                .Host = core::git_host_id{22},
                .Account = core::github_account_id{1001},
                .Repository =
                    {
                        .Owner = "octo-org",
                        .Name = "review-repository",
                    },
                .Number = core::pull_request_number{17},
                .ExpectedRepository = core::repository_id{2001},
                .ExpectedPullRequest = core::pull_request_id{3001},
                .Capabilities = core::capability_revision{44},
                .IdentityGeneration =
                    core::github_identity_generation{3},
                .Generation = core::request_generation{1},
            },
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{22},
        .Account = core::github_account_id{1001},
        .Repository = core::repository_id{2001},
        .PullRequest = core::pull_request_id{3001},
        .Snapshot = core::provider_snapshot_id{55},
        .CapabilityRevision = core::capability_revision{44},
        .Capabilities = capabilities(),
        .RateLimits = canonical_rate_limits(),
        .Limits = {},
        .SearchTerms = "is:open",
    };
}

[[nodiscard]] auto canonical_normalized_read() -> normalized_github_read
{
    const application::github_capability_evidence evidence{
        .Capabilities = capabilities(),
        .Revision = core::capability_revision{44},
        .ReadShape =
            {
                .ReviewThreads = true,
                .ViewedFileState = true,
                .ResolvePermission = true,
                .UnresolvePermission = true,
            },
        .ContractVersion = 1,
    };
    const auto rate_limits = canonical_rate_limits();

    normalized_github_read result;
    result.Session = {
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{22},
        .Viewer =
            {
                .Account = core::github_account_id{1001},
                .Node = application::github_actor_id{"U_fixture_viewer"},
                .Login = "octo-reviewer",
                .DisplayName = "Octo Reviewer",
            },
        .CapabilityEvidence = evidence,
        .RateLimits = rate_limits,
    };
    result.PullRequest = {
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{22},
        .Account = core::github_account_id{1001},
        .Viewer =
            {
                .Actor =
                    application::github_actor_id{"U_fixture_viewer"},
                .Login = "octo-reviewer",
            },
        .Snapshot = core::provider_snapshot_id{55},
        .Repository =
            {
                .Repository = core::repository_id{2001},
                .Node =
                    application::github_node_id{"R_fixture_repository"},
                .Owner = "octo-org",
                .Name = "review-repository",
                .DefaultBranch = "main",
                .Private = false,
            },
        .PullRequest =
            {
                .PullRequest = core::pull_request_id{3001},
                .Repository = core::repository_id{2001},
                .Number = core::pull_request_number{17},
                .Node = application::github_node_id{"PR_fixture_17"},
                .Title = "Keep review snapshots deterministic",
                .Body =
                    "This synthetic pull request exercises the "
                    "normalized read contract.",
                .State = application::github_pull_request_state::open,
                .Draft = false,
                .Author =
                    application::github_actor_record{
                        .Actor =
                            application::github_actor_id{
                                "U_fixture_author"},
                        .Login = "octo-author",
                    },
                .BaseOid =
                    application::commit_oid{std::string{base_oid}},
                .HeadOid =
                    application::commit_oid{std::string{head_oid}},
                .BaseRef = "main",
                .HeadRef = "fixture/deterministic-snapshots",
            },
        .Files =
            {
                {
                    .Path = "src/core/review_snapshot.cpp",
                    .PreviousPath = std::nullopt,
                    .Change = application::github_file_change::modified,
                    .Additions = 12,
                    .Deletions = 3,
                    .ContentKind =
                        application::github_file_content_kind::unknown,
                    .ViewedState =
                        application::github_file_viewed_state::viewed,
                },
            },
        .Commits =
            {
                {
                    .Oid =
                        application::commit_oid{std::string{head_oid}},
                    .Node = application::github_node_id{"C_fixture_head"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "U_fixture_author"},
                            .Login = "octo-author",
                        },
                    .Headline = "Keep review snapshots deterministic",
                },
            },
        .Reviews =
            {
                {
                    .Review =
                        application::github_review_id{"PRR_fixture_1"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "U_fixture_viewer"},
                            .Login = "octo-reviewer",
                        },
                    .State =
                        application::github_review_state::
                            changes_requested,
                    .Commit =
                        application::commit_oid{std::string{head_oid}},
                    .Body =
                        "Please preserve snapshot identity across "
                        "refreshes.",
                },
            },
        .ReviewThreadsAvailability =
            application::github_collection_availability::available,
        .ReviewThreads =
            {
                {
                    .Thread =
                        application::github_thread_id{"PRRT_fixture_1"},
                    .Subject =
                        application::github_review_thread_subject::line,
                    .Path = "src/core/review_snapshot.cpp",
                    .Line = 42,
                    .Resolved = false,
                    .Outdated = false,
                    .ViewerResolvePermission =
                        application::github_permission_state::allowed,
                    .ViewerUnresolvePermission =
                        application::github_permission_state::denied,
                    .Comments =
                        {
                            {
                                .Comment =
                                    application::github_comment_id{
                                        "PRRC_fixture_1"},
                                .ReplyTo = std::nullopt,
                                .Author =
                                    application::github_actor_record{
                                        .Actor =
                                            application::github_actor_id{
                                                "U_fixture_viewer"},
                                        .Login = "octo-reviewer",
                                    },
                                .Body =
                                    "Please preserve snapshot identity "
                                    "across refreshes.",
                                .Path =
                                    "src/core/review_snapshot.cpp",
                                .Line = 42,
                                .Outdated = false,
                                .Deleted = false,
                            },
                        },
                },
            },
        .IssueComments =
            {
                {
                    .Comment =
                        application::github_comment_id{"IC_fixture_1"},
                    .Author =
                        application::github_actor_record{
                            .Actor =
                                application::github_actor_id{
                                    "U_fixture_author"},
                            .Login = "octo-author",
                        },
                    .Body =
                        "Fixture follow-up: validation is complete.",
                    .Deleted = false,
                },
            },
        .Checks =
            {
                {
                    .Check = application::github_check_id{"CR_fixture_1"},
                    .Name = "Local verification",
                    .Status = application::github_check_status::completed,
                    .Conclusion =
                        application::github_check_conclusion::success,
                },
            },
        .SearchResults =
            {
                {
                    .Repository = core::repository_id{2001},
                    .PullRequestNode =
                        application::github_node_id{"PR_fixture_17"},
                    .Number = core::pull_request_number{17},
                    .Title = "Keep review snapshots deterministic",
                    .State = application::github_pull_request_state::open,
                    .Draft = false,
                },
            },
        .CapabilityEvidence = evidence,
        .RateLimits = rate_limits,
    };
    return result;
}

[[nodiscard]] auto execute_recording(
    loaded_recording recording,
    const server_contract server) -> normalized_github_read
{
    const auto compiled_graphql = compile_read_request(
        semantic_read_request{
            .Server = server,
            .Operation = read_operation::list_review_threads,
            .Owner = "octo-org",
            .Repository = "review-repository",
            .PullRequestNumber = 17,
            .HeadOid = {},
            .SearchTerms = {},
            .Page = 1,
            .GraphqlPage =
                github_graphql_page_request{
                    .Kind = github_graphql_page_kind::initial,
                    .Shape =
                        {
                            .ReviewThreads = true,
                            .ViewedFileState = true,
                            .ResolvePermission = true,
                            .UnresolvePermission = true,
                        },
                    .AfterCursor = {},
                    .ThreadId = {},
                },
        });
    expect(compiled_graphql.has_value(), "fixture GraphQL request compiles");
    expect(
        compiled_graphql->Body
            == recording.Exchanges[6].Request.Body,
        "compiled GraphQL body matches the manifest exactly");
    strict_recorded_transport transport{std::move(recording.Exchanges)};
    const auto result =
        perform_normalized_github_read(
            transport,
            normalization_context(server));
    if(!result.has_value())
    {
        std::cerr << "normalization failure code: "
                  << static_cast<int>(result.error().Code) << '\n';
    }
    expect(result.has_value(), "recorded provider facade normalizes");
    expect(transport.consumed_all(), "all ten manifest reads are consumed");
    return *result;
}

auto test_manifest_catalog_and_normalization_are_one_contract() -> void
{
    const auto cloud = execute_recording(
        load_recording("github-com", server_contract::github_com),
        server_contract::github_com);
    const auto enterprise = execute_recording(
        load_recording("ghes-3.18", server_contract::ghes_3_18),
        server_contract::ghes_3_18);
    const auto expected = canonical_normalized_read();
    expect(cloud == expected, "GitHub.com matches independent C++ facts");
    expect(enterprise == expected, "GHES 3.18 matches independent C++ facts");
    expect(cloud == enterprise, "paired hosts normalize identically");
}

[[nodiscard]] auto semantic(
    const read_operation operation,
    const server_contract server = server_contract::github_com)
    -> semantic_read_request
{
    semantic_read_request request{
        .Server = server,
        .Operation = operation,
        .Owner = {},
        .Repository = {},
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1,
        .GraphqlPage = std::nullopt,
    };
    if(operation != read_operation::get_viewer_identity)
    {
        request.Owner = "octo-org";
        request.Repository = "review-repository";
    }
    switch(operation)
    {
    case read_operation::get_pull_request:
    case read_operation::list_pull_request_files:
    case read_operation::list_pull_request_commits:
    case read_operation::list_reviews:
    case read_operation::list_review_threads:
    case read_operation::list_issue_comments:
        request.PullRequestNumber = 17;
        break;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::list_checks:
    case read_operation::search_pull_requests:
        break;
    }
    if(operation == read_operation::list_checks)
    {
        request.HeadOid = std::string{head_oid};
    }
    if(operation == read_operation::search_pull_requests)
    {
        request.SearchTerms = "is:open";
    }
    return request;
}

[[nodiscard]] auto paged_graphql_semantic(
    const github_graphql_page_kind kind,
    const std::uint32_t page,
    const application::github_read_shape shape,
    std::string after_cursor = {},
    std::string thread_id = {}) -> semantic_read_request
{
    auto request = semantic(read_operation::list_review_threads);
    request.Page = page;
    request.GraphqlPage = github_graphql_page_request{
        .Kind = kind,
        .Shape = shape,
        .AfterCursor = std::move(after_cursor),
        .ThreadId = std::move(thread_id),
    };
    return request;
}

[[nodiscard]] auto graphql_request_shapes()
    -> std::vector<semantic_read_request>
{
    return {
        semantic(read_operation::list_review_threads),
        paged_graphql_semantic(
            github_graphql_page_kind::initial,
            1U,
            {
                .ReviewThreads = true,
                .ViewedFileState = true,
                .ResolvePermission = true,
                .UnresolvePermission = true,
            }),
        paged_graphql_semantic(
            github_graphql_page_kind::files,
            2U,
            {
                .ReviewThreads = false,
                .ViewedFileState = true,
                .ResolvePermission = false,
                .UnresolvePermission = false,
            },
            "fixture-files-cursor"),
        paged_graphql_semantic(
            github_graphql_page_kind::threads,
            2U,
            {
                .ReviewThreads = true,
                .ViewedFileState = false,
                .ResolvePermission = true,
                .UnresolvePermission = true,
            },
            "fixture-threads-cursor"),
        paged_graphql_semantic(
            github_graphql_page_kind::comments,
            2U,
            {
                .ReviewThreads = true,
                .ViewedFileState = false,
                .ResolvePermission = false,
                .UnresolvePermission = false,
            },
            "fixture-comments-cursor",
            "PRRT_fixture_thread"),
    };
}

[[nodiscard]] auto graphql_query(const wire_request& request) -> std::string
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        QByteArray::fromStdString(request.Body),
        &error);
    expect(
        error.error == QJsonParseError::NoError && document.isObject()
            && document.object().value("query").isString(),
        "compiled GraphQL body exposes one query string");
    return utf8(document.object().value("query").toString());
}

[[nodiscard]] auto with_graphql_query(
    wire_request request,
    const std::string_view query) -> wire_request
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        QByteArray::fromStdString(request.Body),
        &error);
    expect(
        error.error == QJsonParseError::NoError && document.isObject(),
        "hostile GraphQL base body is valid JSON");
    auto object = document.object();
    object.insert(
        "query",
        QString::fromUtf8(
            query.data(),
            static_cast<qsizetype>(query.size())));
    request.Body =
        QJsonDocument{object}.toJson(QJsonDocument::Compact).toStdString();
    return request;
}

auto expect_graphql_failure(
    const wire_request& request,
    const contract_failure_code expected,
    const std::string_view message) -> void
{
    const auto result = validate_outbound_read_request(request);
    expect(
        !result.has_value() && result.error().Code == expected,
        message);
}

[[nodiscard]] auto nested_query(const std::size_t depth) -> std::string
{
    auto query = std::string{"query Deep"};
    for(std::size_t index = 0U; index < depth; ++index)
    {
        query += "{field";
    }
    query.append(depth, '}');
    return query;
}

[[nodiscard]] auto query_with_definitions(
    const std::size_t definitions) -> std::string
{
    auto query = std::string{"query Safe{name}"};
    for(std::size_t index = 1U; index < definitions; ++index)
    {
        query += " fragment F";
        query += std::to_string(index);
        query += " on Type{name}";
    }
    return query;
}

auto test_read_only_protocol_policy() -> void
{
    const auto rest = compile_read_request(
        semantic(read_operation::get_repository));
    expect(rest.has_value(), "raw-method REST fixture compiles");

    constexpr std::array raw_methods{
        std::string_view{"GET"},
        std::string_view{"HEAD"},
        std::string_view{"POST"},
        std::string_view{"PUT"},
        std::string_view{"PATCH"},
        std::string_view{"DELETE"},
        std::string_view{"OPTIONS"},
        std::string_view{"TRACE"},
        std::string_view{"CONNECT"},
        std::string_view{"UNKNOWN"},
        std::string_view{"get"},
        std::string_view{"post"},
        std::string_view{"PoSt"},
        std::string_view{""},
        std::string_view{" GET"},
        std::string_view{"GET "},
    };
    for(const auto raw_method : raw_methods)
    {
        const auto parsed = parse_recorded_http_method(raw_method);
        auto accepted = false;
        if(parsed.has_value())
        {
            auto candidate = *rest;
            candidate.Method = *parsed;
            accepted =
                validate_outbound_read_request(candidate).has_value();
        }
        expect(
            accepted == (raw_method == "GET"),
            "raw REST sink accepts only exact GET");
        expect(
            parsed.has_value()
                == (raw_method == "GET" || raw_method == "POST"),
            "recording method parser accepts only exact GET or POST");
    }
    expect(
        parse_recorded_http_method("GET") == http_method::get
            && parse_recorded_http_method("POST") == http_method::post,
        "recording methods retain their exact typed meaning");

    using policy = graphql_operation_policy_result;
    expect(
        inspect_graphql_operation_policy(
            "# mutation is comment text\n"
            "query mutation($value:String=\"mutation\"){"
            "mutationType probe(value:\"mutation\"){name}} "
            "fragment MutationFields on __Type{name}")
            == policy::query_only,
        "comments, strings, field names, operation names, and fragments do "
        "not create write operations");
    expect(
        inspect_graphql_operation_policy(
            "query Safe($value:String=\"\"\"mutation\"\"\"){"
            "probe(value:$value){name}}")
            == policy::query_only,
        "block strings containing mutation remain query data");
    expect(
        inspect_graphql_operation_policy(
            "query Safe{probe(integer:-17,zero:0,float:1.25,"
            "exponent:-2.5e+3){name}}")
            == policy::query_only,
        "valid integer, float, and exponent values remain query data");
    expect(
        inspect_graphql_operation_policy(
            "mutation Unsafe{markFileAsViewed(input:{}){clientMutationId}}")
            == policy::write_operation
            && inspect_graphql_operation_policy(
                   "subscription Unsafe{viewer{login}}")
                == policy::write_operation
            && inspect_graphql_operation_policy(
                   "query Safe{viewer{login}} "
                   "mutation Hidden{__typename}")
                == policy::write_operation,
        "top-level mutation, subscription, and hidden writes are rejected");
    expect(
        inspect_graphql_operation_policy(
            "query First{viewer{login}} query Second{__typename}")
            == policy::multiple_operations,
        "multiple read operations fail closed without operation selection");
    expect(
        inspect_graphql_operation_policy("query Broken{viewer{login}")
                == policy::malformed
            && inspect_graphql_operation_policy(
                   "query Broken{probe(value:\"unterminated){name}}")
                == policy::malformed
            && inspect_graphql_operation_policy(
                   "query Safe{viewer{login}} trailing")
                == policy::malformed,
        "unbalanced, unterminated, and trailing definitions fail closed");
    constexpr std::array malformed_numeric_queries{
        std::string_view{"query Broken{probe(value:-){name}}"},
        std::string_view{"query Broken{probe(value:01){name}}"},
        std::string_view{"query Broken{probe(value:1.){name}}"},
        std::string_view{"query Broken{probe(value:1e+){name}}"},
        std::string_view{"query Broken{probe(value:1name){name}}"},
    };
    for(const auto query : malformed_numeric_queries)
    {
        expect(
            inspect_graphql_operation_policy(query) == policy::malformed,
            "malformed GraphQL numeric syntax fails closed");
    }

    auto oversized_query = std::string{"query Oversized{"};
    oversized_query.append(
        maximum_graphql_operation_document_bytes,
        'a');
    oversized_query.push_back('}');
    expect(
        inspect_graphql_operation_policy(oversized_query)
                == policy::response_limit
            && inspect_graphql_operation_policy(
                   nested_query(maximum_graphql_operation_nesting))
                == policy::query_only
            && inspect_graphql_operation_policy(
                   nested_query(maximum_graphql_operation_nesting + 1U))
                == policy::response_limit
            && inspect_graphql_operation_policy(
                   query_with_definitions(
                       maximum_graphql_document_definitions))
                == policy::query_only
            && inspect_graphql_operation_policy(
                   query_with_definitions(
                       maximum_graphql_document_definitions + 1U))
                == policy::response_limit,
        "GraphQL operation policy bounds bytes, nesting, and definitions");

    for(const auto& semantic_request : graphql_request_shapes())
    {
        const auto compiled = compile_read_request(semantic_request);
        expect(
            compiled.has_value(),
            "every legacy and paged GraphQL request shape compiles");
        const auto query = graphql_query(*compiled);
        expect(
            inspect_graphql_operation_policy(query)
                == policy::query_only,
            "every compiled GraphQL request shape is exactly one query");

        auto altered_read = query;
        expect(
            !altered_read.empty() && altered_read.back() == '}',
            "compiled GraphQL query has an outer selection terminator");
        altered_read.insert(
            altered_read.size() - 1U,
            " __typename");
        expect(
            inspect_graphql_operation_policy(altered_read)
                == policy::query_only,
            "noncanonical read remains exactly one scanner-approved query");
        expect_graphql_failure(
            with_graphql_query(*compiled, altered_read),
            contract_failure_code::unexpected_body,
            "canonical body equality rejects a scanner-approved altered "
            "selection");

        auto mutation = query;
        mutation.replace(0U, std::string_view{"query"}.size(), "mutation");
        expect_graphql_failure(
            with_graphql_query(*compiled, mutation),
            contract_failure_code::mutation_operation,
            "every GraphQL request shape rejects a mutation operation");

        auto subscription = query;
        subscription.replace(
            0U,
            std::string_view{"query"}.size(),
            "subscription");
        expect_graphql_failure(
            with_graphql_query(*compiled, subscription),
            contract_failure_code::mutation_operation,
            "every GraphQL request shape rejects a subscription operation");
        expect_graphql_failure(
            with_graphql_query(
                *compiled,
                query + " mutation Hidden{__typename}"),
            contract_failure_code::mutation_operation,
            "every GraphQL request shape rejects a hidden mutation");
        expect_graphql_failure(
            with_graphql_query(
                *compiled,
                query + " query Duplicate{__typename}"),
            contract_failure_code::unexpected_body,
            "every GraphQL request shape rejects multiple operations");

        auto malformed = query;
        malformed.pop_back();
        expect_graphql_failure(
            with_graphql_query(*compiled, malformed),
            contract_failure_code::unexpected_body,
            "every GraphQL request shape rejects malformed structure");
    }

    const auto legacy = compile_read_request(
        semantic(read_operation::list_review_threads));
    expect(legacy.has_value(), "oversized GraphQL base request compiles");
    expect_graphql_failure(
        with_graphql_query(*legacy, oversized_query),
        contract_failure_code::response_limit,
        "outbound GraphQL operation policy exposes its document bound");
}

auto test_read_only_catalog_and_outbound_validation() -> void
{
    expect(
        read_operation_catalog.size() == 10U,
        "catalog has exactly ten reads");
    for(const auto operation : read_operation_catalog)
    {
        const auto round_trip =
            read_operation_from_name(operation_name(operation));
        expect(
            round_trip.has_value() && *round_trip == operation,
            "every read name round-trips");
    }
    for(const auto mutation :
        {
            "MarkFileViewed",
            "MarkFileUnviewed",
            "CreateReview",
            "SubmitReview",
            "CreateThread",
            "ReplyToThread",
            "ResolveThread",
            "UnresolveThread",
        })
    {
        const auto result = read_operation_from_name(mutation);
        expect(
            !result.has_value()
                && result.error().Code
                    == contract_failure_code::mutation_operation,
            "mutation operation is explicitly rejected");
    }

    const auto rest = compile_read_request(
        semantic(read_operation::get_repository));
    expect(rest.has_value(), "REST request compiles");
    expect(
        rest->Headers
            == std::vector<http_header>{
                {"Accept", "application/vnd.github+json"},
                {"User-Agent", "BetterPullReview/0.1"},
                {"X-GitHub-Api-Version", "2022-11-28"},
            },
        "REST request uses exact safe headers");

    const auto graphql = compile_read_request(
        semantic(read_operation::list_review_threads));
    expect(graphql.has_value(), "GraphQL request compiles");
    expect(
        graphql->Headers
            == std::vector<http_header>{
                {"Accept", "application/json"},
                {"Content-Type", "application/json"},
                {"User-Agent", "BetterPullReview/0.1"},
            },
        "GraphQL does not claim REST versioning");

    auto hostile = *rest;
    hostile.Path = "/user";
    expect(
        !validate_outbound_read_request(hostile).has_value(),
        "operation-specific path drift is rejected");
    hostile = *rest;
    hostile.Query = "page=2";
    expect(
        !validate_outbound_read_request(hostile).has_value(),
        "unexpected query is rejected");
    hostile = *rest;
    hostile.Headers.push_back({"X-Injected", "value"});
    expect(
        !validate_outbound_read_request(hostile).has_value(),
        "extra header is rejected");
    hostile = *rest;
    hostile.Method = http_method::post;
    expect(
        !validate_outbound_read_request(hostile).has_value(),
        "REST write method is rejected");

    auto injected_search =
        semantic(read_operation::search_pull_requests);
    injected_search.SearchTerms = "repo:other is:issue";
    expect(
        !compile_read_request(injected_search).has_value(),
        "structured search cannot inject repository or item kind");

    auto oversized_viewer =
        semantic(read_operation::get_viewer_identity);
    oversized_viewer.Owner.assign(1'000'000U, 'a');
    expect(
        !compile_read_request(oversized_viewer).has_value(),
        "irrelevant semantic fields are bounded before copying");

    auto aliased_repository =
        semantic(read_operation::get_repository);
    aliased_repository.Page = 2;
    expect(
        !compile_read_request(aliased_repository).has_value(),
        "nonpaged REST reads have exactly one semantic page identity");

    auto aliased_graphql =
        semantic(read_operation::list_review_threads);
    aliased_graphql.Page = 2;
    expect(
        !compile_read_request(aliased_graphql).has_value(),
        "uncursored GraphQL reads cannot alias page one");

    auto irrelevant_head =
        semantic(read_operation::get_pull_request);
    irrelevant_head.HeadOid = std::string{head_oid};
    expect(
        !compile_read_request(irrelevant_head).has_value(),
        "irrelevant commit identity cannot alias a PR metadata read");

    auto excessive_search_page =
        semantic(read_operation::search_pull_requests);
    excessive_search_page.Page = 11;
    expect(
        !compile_read_request(excessive_search_page).has_value(),
        "search page identity stops at GitHub's one-thousand-result cap");

    auto excessive_commit_page =
        semantic(read_operation::list_pull_request_commits);
    excessive_commit_page.Page = 4;
    expect(
        !compile_read_request(excessive_commit_page).has_value(),
        "commit page identity stops at GitHub's 250-commit cap");

    auto maximum_graphql_number =
        semantic(read_operation::get_pull_request);
    maximum_graphql_number.PullRequestNumber =
        core::maximum_pull_request_number;
    expect(
        compile_read_request(maximum_graphql_number).has_value(),
        "GraphQL-compatible PR number at N compiles");
    maximum_graphql_number.PullRequestNumber =
        core::maximum_pull_request_number + 1U;
    expect(
        !compile_read_request(maximum_graphql_number).has_value(),
        "PR number above GraphQL Int range is rejected");
}

auto replace_once(
    std::string& text,
    const std::string_view from,
    const std::string_view to) -> void
{
    const auto position = text.find(from);
    expect(position != std::string::npos, "hostile replacement marker exists");
    text.replace(position, from.size(), to);
}

auto mark_graphql_page_incomplete(
    std::string& body,
    const std::string_view cursor) -> void
{
    const auto cursor_position = body.find(cursor);
    expect(
        cursor_position != std::string::npos,
        "targeted GraphQL cursor exists");
    const auto marker = std::string_view{"\"hasNextPage\": false"};
    const auto marker_position = body.rfind(marker, cursor_position);
    expect(
        marker_position != std::string::npos,
        "targeted GraphQL page marker exists");
    body.replace(
        marker_position,
        marker.size(),
        "\"hasNextPage\": true");
}

auto expect_atomic_failure(
    const github_read_normalization_context& context,
    const std::vector<operation_response>& responses,
    const std::string_view message) -> void
{
    const auto result = normalize_github_read(context, responses);
    expect(!result.has_value(), message);
}

auto test_hostile_responses_fail_atomically() -> void
{
    auto recording =
        load_recording("github-com", server_contract::github_com);
    const auto context = normalization_context(server_contract::github_com);

    auto responses = recording.Responses;
    replace_once(
        responses[0].Response.Body,
        "\"id\": 1001,",
        "\"id\": 1001,\n  \"id\": 1001,");
    expect_atomic_failure(
        context,
        responses,
        "duplicate JSON member yields no candidate snapshot");

    responses = recording.Responses;
    replace_once(
        responses[2].Response.Body,
        "\"id\": 3001,",
        "\"id\": 9999,");
    expect_atomic_failure(
        context,
        responses,
        "cross-response pull-request identity mismatch is atomic");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"id\": \"R_fixture_repository\"",
        "\"id\": \"R_replayed_repository\"");
    expect_atomic_failure(
        context,
        responses,
        "GraphQL repository identity is bound to the REST repository");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"id\": \"PR_fixture_17\"",
        "\"id\": \"PR_replayed_18\"");
    expect_atomic_failure(
        context,
        responses,
        "GraphQL pull-request identity is bound to REST metadata");

    responses = recording.Responses;
    replace_once(
        responses[9].Response.Body,
        "\"number\": 17",
        "\"number\": 18");
    expect_atomic_failure(
        context,
        responses,
        "search node and repository-scoped PR number cannot disagree");

    responses = recording.Responses;
    replace_once(
        responses[9].Response.Body,
        "\"title\": \"Keep review snapshots deterministic\"",
        "\"title\": \"Contradictory search title\"");
    expect_atomic_failure(
        context,
        responses,
        "search metadata cannot contradict the same pull request");

    responses = recording.Responses;
    replace_once(
        responses[9].Response.Body,
        "\"draft\": false",
        "\"draft\": true");
    expect_atomic_failure(
        context,
        responses,
        "search draft state cannot contradict the same pull request");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"login\": \"octo-reviewer\"",
        "\"login\": \"identity-conflict\"");
    expect_atomic_failure(
        context,
        responses,
        "one actor ID cannot carry conflicting logins");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"bodyText\": \"Please preserve snapshot identity across "
        "refreshes.\",\n"
        "                    \"path\": \"src/core/review_snapshot.cpp\"",
        "\"bodyText\": \"Please preserve snapshot identity across "
        "refreshes.\",\n"
        "                    \"path\": \"src/core/other.cpp\"");
    expect_atomic_failure(
        context,
        responses,
        "review comment path must match its thread");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"path\": \"src/core/review_snapshot.cpp\",\n"
        "              \"viewerViewedState\": \"VIEWED\"",
        "\"path\": \"src/core/other.cpp\",\n"
        "              \"viewerViewedState\": \"VIEWED\"");
    expect_atomic_failure(
        context,
        responses,
        "GraphQL viewed state must correlate to one REST file path");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"viewerViewedState\": \"VIEWED\"",
        "\"viewerViewedState\": \"UNKNOWN\"");
    expect_atomic_failure(
        context,
        responses,
        "unknown viewed-file state is rejected");

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"viewerCanResolve\": true",
        "\"viewerCanResolve\": null");
    expect_atomic_failure(
        context,
        responses,
        "non-boolean per-thread resolve availability is rejected");

    responses = recording.Responses;
    replace_once(
        responses[9].Response.Body,
        "\"state\": \"open\"",
        "\"state\": \"closed\"");
    expect_atomic_failure(
        context,
        responses,
        "open-only search cannot normalize a closed result");

    responses = recording.Responses;
    replace_once(
        responses[9].Response.Body,
        "\"state\": \"open\"",
        "\"state\": \"unknown-provider-state\"");
    expect_atomic_failure(
        context,
        responses,
        "unknown provider enum is rejected");

    for(const auto& [cursor, description] : {
            std::pair{
                std::string_view{"Y3Vyc29yOlBSQ0ZfZml4dHVyZV8x"},
                std::string_view{"viewed-file"},
            },
            std::pair{
                std::string_view{"Y3Vyc29yOlBSUkNfZml4dHVyZV8x"},
                std::string_view{"thread-comment"},
            },
            std::pair{
                std::string_view{"Y3Vyc29yOlBSUlRfZml4dHVyZV8x"},
                std::string_view{"review-thread"},
            },
        })
    {
        responses = recording.Responses;
        mark_graphql_page_incomplete(
            responses[6].Response.Body,
            cursor);
        const auto strict = validate_strict_fixture_response(
            read_operation::list_review_threads,
            responses[6].Response);
        expect(
            !strict.has_value()
                && strict.error().Code
                    == contract_failure_code::response_limit,
            "each incomplete GraphQL connection is classified as a limit");
        expect_atomic_failure(
            context,
            responses,
            std::string{"incomplete "} + std::string{description}
                + " GraphQL page cannot become a snapshot");
    }

    responses = recording.Responses;
    replace_once(
        responses[6].Response.Body,
        "\"line\": 42",
        "\"line\": 2147483648");
    expect_atomic_failure(
        context,
        responses,
        "GraphQL line above signed Int range is rejected");

    responses = recording.Responses;
    auto& file_body = responses[3].Response.Body;
    const auto original_file = file_body.substr(
        file_body.find('{'),
        file_body.rfind('}') - file_body.find('{') + 1U);
    file_body = "[";
    for(std::size_t index = 0; index < 101U; ++index)
    {
        if(index != 0U)
        {
            file_body += ',';
        }
        file_body += original_file;
    }
    file_body += ']';
    expect_atomic_failure(
        context,
        responses,
        "page at N+1 is rejected before DOM allocation");

    responses = recording.Responses;
    responses[3].Response.Completeness =
        page_completeness::has_next_page;
    expect_atomic_failure(
        context,
        responses,
        "transport-declared REST continuation cannot become a snapshot");

    responses = recording.Responses;
    responses[3].Response.Completeness = page_completeness::unknown;
    expect_atomic_failure(
        context,
        responses,
        "response with unknown page completeness fails closed");

    responses = recording.Responses;
    responses[3].Response.Completeness =
        static_cast<page_completeness>(255);
    expect_atomic_failure(
        context,
        responses,
        "malformed page completeness fails closed");

    responses = recording.Responses;
    replace_once(
        responses[5].Response.Body,
        "\"node_id\": \"PRR_fixture_1\"",
        "\"node_id\": \"R_fixture_repository\"");
    expect_atomic_failure(
        context,
        responses,
        "GitHub node identity cannot collide across entity kinds");

    auto mismatched_context = context;
    mismatched_context.Account = core::github_account_id{9999};
    expect_atomic_failure(
        mismatched_context,
        recording.Responses,
        "redundant authority mismatch cannot launder provider data");

    auto aliased_search_context = context;
    aliased_search_context.SearchTerms = "is:closed";
    expect_atomic_failure(
        aliased_search_context,
        recording.Responses,
        "normalization context cannot alias the selected open search");

    responses = recording.Responses;
    responses.push_back(responses.front());
    expect_atomic_failure(
        context,
        responses,
        "extra operation response is rejected");
}

auto test_provider_valid_nullable_and_conditional_shapes() -> void
{
    auto recording =
        load_recording("github-com", server_contract::github_com);

    expect(
        validate_strict_fixture_response(
            read_operation::list_review_threads,
            recording.Responses[6].Response)
            .has_value(),
        "nonempty final GraphQL pages retain non-null cursors");

    wire_response empty_threads{
        .Status = 200,
        .Headers = {{"Content-Type", "application/json"}},
        .Body =
            R"({"data":{"repository":{"id":"R_fixture_repository","pullRequest":{"id":"PR_fixture_17","files":{"totalCount":0,"nodes":[],"pageInfo":{"hasNextPage":false,"endCursor":null}},"reviewThreads":{"totalCount":0,"nodes":[],"pageInfo":{"hasNextPage":false,"endCursor":null}}}}}})",
        .Completeness = page_completeness::complete,
    };
    expect(
        validate_strict_fixture_response(
            read_operation::list_review_threads,
            empty_threads)
            .has_value(),
        "empty final GraphQL page has a null cursor");

    auto missing_final_cursor = recording.Responses[6].Response;
    replace_once(
        missing_final_cursor.Body,
        "\"endCursor\": \"Y3Vyc29yOlBSUkNfZml4dHVyZV8x\"",
        "\"endCursor\": null");
    expect(
        !validate_strict_fixture_response(
            read_operation::list_review_threads,
            missing_final_cursor)
             .has_value(),
        "nonempty final GraphQL page rejects a missing cursor");

    auto viewer = recording.Responses[0].Response;
    replace_once(
        viewer.Body,
        "\"name\": \"Octo Reviewer\"",
        "\"name\": null");
    expect(
        validate_strict_fixture_response(
            read_operation::get_viewer_identity,
            viewer)
            .has_value(),
        "nullable viewer display name is valid");

    auto pull_request = recording.Responses[2].Response;
    replace_once(
        pull_request.Body,
        "\"body\": \"This synthetic pull request exercises the "
        "normalized read contract.\"",
        "\"body\": null");
    expect(
        validate_strict_fixture_response(
            read_operation::get_pull_request,
            pull_request)
            .has_value(),
        "nullable PR body is valid");

    auto fork_pull_request = recording.Responses[2].Response;
    const auto head_position = fork_pull_request.Body.find("\"head\"");
    expect(head_position != std::string::npos, "head fixture exists");
    auto repository_id_position =
        fork_pull_request.Body.find("\"id\": 2001", head_position);
    expect(
        repository_id_position != std::string::npos,
        "head repository ID exists");
    fork_pull_request.Body.replace(
        repository_id_position,
        std::string_view{"\"id\": 2001"}.size(),
        "\"id\": 9002");
    auto full_name_position = fork_pull_request.Body.find(
        "octo-org/review-repository",
        head_position);
    expect(
        full_name_position != std::string::npos,
        "head repository name exists");
    fork_pull_request.Body.replace(
        full_name_position,
        std::string_view{"octo-org/review-repository"}.size(),
        "octo-fork/review-repository");
    expect(
        validate_strict_fixture_response(
            read_operation::get_pull_request,
            fork_pull_request)
            .has_value(),
        "fork head repository need not equal the base repository");
    recording.Responses[2].Response = std::move(fork_pull_request);
    expect(
        normalize_github_read(
            normalization_context(server_contract::github_com),
            recording.Responses)
            .has_value(),
        "fork head cannot be rejected by base authority correlation");

    auto review = recording.Responses[5].Response;
    replace_once(
        review.Body,
        "\"state\": \"CHANGES_REQUESTED\"",
        "\"state\": \"PENDING\"");
    expect(
        validate_strict_fixture_response(
            read_operation::list_reviews,
            review)
            .has_value(),
        "pending review is a valid provider state");

    for(const auto& [wire_state, normalized_state] : {
            std::pair{
                std::string_view{"DISMISSED"},
                application::github_file_viewed_state::dismissed,
            },
            std::pair{
                std::string_view{"UNVIEWED"},
                application::github_file_viewed_state::unviewed,
            },
            std::pair{
                std::string_view{"VIEWED"},
                application::github_file_viewed_state::viewed,
            },
        })
    {
        auto viewed_recording = recording;
        replace_once(
            viewed_recording.Responses[6].Response.Body,
            "\"viewerViewedState\": \"VIEWED\"",
            std::string{"\"viewerViewedState\": \""}
                + std::string{wire_state} + "\"");
        const auto normalized = normalize_github_read(
            normalization_context(server_contract::github_com),
            viewed_recording.Responses);
        expect(
            normalized.has_value()
                && normalized->PullRequest.Files.front().ViewedState
                    == normalized_state,
            "every selected viewed-file state normalizes exactly");
    }

    auto target_availability = recording;
    replace_once(
        target_availability.Responses[6].Response.Body,
        "\"viewerCanResolve\": true",
        "\"viewerCanResolve\": false");
    replace_once(
        target_availability.Responses[6].Response.Body,
        "\"viewerCanUnresolve\": false",
        "\"viewerCanUnresolve\": true");
    const auto normalized_availability = normalize_github_read(
        normalization_context(server_contract::github_com),
        target_availability.Responses);
    expect(
        normalized_availability.has_value()
            && normalized_availability->PullRequest.ReviewThreads.front()
                       .ViewerResolvePermission
                == application::github_permission_state::denied
            && normalized_availability->PullRequest.ReviewThreads.front()
                       .ViewerUnresolvePermission
                == application::github_permission_state::allowed
            && normalized_availability->PullRequest.CapabilityEvidence
                       .Capabilities
                == capabilities(),
        "per-thread availability remains distinct from host capability");

    auto check = recording.Responses[8].Response;
    replace_once(
        check.Body,
        "\"status\": \"completed\"",
        "\"status\": \"queued\"");
    replace_once(
        check.Body,
        "\"conclusion\": \"success\"",
        "\"conclusion\": null");
    expect(
        validate_strict_fixture_response(
            read_operation::list_checks,
            check)
            .has_value(),
        "queued check has nullable completion facts");

    auto file = recording.Responses[3].Response;
    replace_once(
        file.Body,
        "\"status\": \"modified\"",
        "\"status\": \"renamed\",\n"
        "    \"previous_filename\": \"src/core/old_snapshot.cpp\"");
    expect(
        validate_strict_fixture_response(
            read_operation::list_pull_request_files,
            file)
            .has_value(),
        "renamed file accepts an available previous path");

    auto copied_file = recording.Responses[3].Response;
    replace_once(
        copied_file.Body,
        "\"status\": \"modified\"",
        "\"status\": \"copied\",\n"
        "    \"previous_filename\": \"src/core/source_snapshot.cpp\"");
    expect(
        validate_strict_fixture_response(
            read_operation::list_pull_request_files,
            copied_file)
            .has_value(),
        "copied file accepts an available source path");

    auto unchanged_file = recording.Responses[3].Response;
    replace_once(
        unchanged_file.Body,
        "\"status\": \"modified\"",
        "\"status\": \"unchanged\"");
    expect(
        validate_strict_fixture_response(
            read_operation::list_pull_request_files,
            unchanged_file)
            .has_value(),
        "unchanged is a valid GitHub file status");

    auto nullable_commit_recording =
        load_recording("github-com", server_contract::github_com);
    replace_once(
        nullable_commit_recording.Responses[5].Response.Body,
        "\"commit_id\": \"2222222222222222222222222222222222222222\"",
        "\"commit_id\": null");
    const auto nullable_commit = normalize_github_read(
        normalization_context(server_contract::github_com),
        nullable_commit_recording.Responses);
    expect(
        nullable_commit.has_value()
            && !nullable_commit->PullRequest.Reviews.front()
                    .Commit.has_value(),
        "a review whose commit was deleted keeps a nullable commit");

    auto empty_message_recording =
        load_recording("github-com", server_contract::github_com);
    replace_once(
        empty_message_recording.Responses[4].Response.Body,
        "\"message\": \"Keep review snapshots deterministic\"",
        "\"message\": \"\"");
    const auto empty_message = normalize_github_read(
        normalization_context(server_contract::github_com),
        empty_message_recording.Responses);
    expect(
        empty_message.has_value()
            && empty_message->PullRequest.Commits.front()
                    .Headline.empty(),
        "Git commits may have an empty message");

    auto file_thread_recording =
        load_recording("github-com", server_contract::github_com);
    auto& file_thread =
        file_thread_recording.Responses[6].Response.Body;
    replace_once(
        file_thread,
        "\"subjectType\": \"LINE\"",
        "\"subjectType\": \"FILE\"");
    replace_once(file_thread, "\"line\": 42", "\"line\": null");
    replace_once(
        file_thread,
        "\"originalLine\": 42",
        "\"originalLine\": null");
    replace_once(file_thread, "\"line\": 42", "\"line\": null");
    replace_once(
        file_thread,
        "\"originalLine\": 42",
        "\"originalLine\": null");
    const auto normalized_file_thread = normalize_github_read(
        normalization_context(server_contract::github_com),
        file_thread_recording.Responses);
    expect(
        normalized_file_thread.has_value()
            && normalized_file_thread->PullRequest.ReviewThreads.front()
                    .Subject
                == application::github_review_thread_subject::file
            && !normalized_file_thread->PullRequest.ReviewThreads.front()
                    .Line.has_value()
            && !normalized_file_thread->PullRequest.ReviewThreads.front()
                    .Comments.front()
                    .Line.has_value(),
        "file-level thread and comment normalize without line numbers");

    auto null_author_recording =
        load_recording("github-com", server_contract::github_com);
    replace_once(
        null_author_recording.Responses[6].Response.Body,
        "\"author\": {\n"
        "                      \"login\": \"octo-reviewer\",\n"
        "                      \"id\": \"U_fixture_viewer\"\n"
        "                    }",
        "\"author\": null");
    const auto null_author = normalize_github_read(
        normalization_context(server_contract::github_com),
        null_author_recording.Responses);
    expect(
        null_author.has_value()
            && !null_author->PullRequest.ReviewThreads.front()
                    .Comments.front()
                    .Author.has_value(),
        "deleted review-comment author remains nullable");

    auto bot_author_recording =
        load_recording("github-com", server_contract::github_com);
    replace_once(
        bot_author_recording.Responses[6].Response.Body,
        "\"login\": \"octo-reviewer\"",
        "\"login\": \"fixture-bot[bot]\"");
    replace_once(
        bot_author_recording.Responses[6].Response.Body,
        "\"id\": \"U_fixture_viewer\"",
        "\"id\": \"BOT_fixture_ci\"");
    const auto bot_author = normalize_github_read(
        normalization_context(server_contract::github_com),
        bot_author_recording.Responses);
    expect(
        bot_author.has_value()
            && bot_author->PullRequest.ReviewThreads.front()
                    .Comments.front()
                    .Author.has_value()
            && bot_author->PullRequest.ReviewThreads.front()
                    .Comments.front()
                    .Author->Actor.value()
                == "BOT_fixture_ci",
        "non-User Node actors retain stable identity");
}
}

auto main() -> int
{
    test_manifest_catalog_and_normalization_are_one_contract();
    test_read_only_protocol_policy();
    test_read_only_catalog_and_outbound_validation();
    test_hostile_responses_fail_atomically();
    test_provider_valid_nullable_and_conditional_shapes();

    std::cout << "All GitHub normalized read-contract tests passed\n";
    return EXIT_SUCCESS;
}
