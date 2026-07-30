#include "github_paged_read.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::adapters::github::detail;

constexpr std::string_view base_oid =
    "1111111111111111111111111111111111111111";
constexpr std::string_view head_oid =
    "2222222222222222222222222222222222222222";

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] auto json(const QJsonObject& value) -> std::string
{
    return QJsonDocument{value}.toJson(QJsonDocument::Compact).toStdString();
}

[[nodiscard]] auto json(const QJsonArray& value) -> std::string
{
    return QJsonDocument{value}.toJson(QJsonDocument::Compact).toStdString();
}

auto fill_to_member_count(QJsonObject& object,
                          const QString& prefix,
                          const qsizetype target) -> void
{
    qsizetype index = 0;
    while(object.size() < target)
    {
        object.insert(QString{"%1_%2"}.arg(prefix).arg(index),
                      QString{"ignored raw GitHub field %1"}.arg(index));
        ++index;
    }
}

[[nodiscard]] auto
response_headers(std::optional<std::string> link = std::nullopt)
    -> std::vector<http_header>
{
    std::vector<http_header> result{
        {
            .Name = "Content-Type",
            .Value = "application/json; charset=utf-8",
        },
    };
    if(link.has_value())
    {
        result.push_back({
            .Name = "Link",
            .Value = std::move(*link),
        });
    }
    result.push_back({
        .Name = "X-RateLimit-Limit",
        .Value = "5000",
    });
    result.push_back({
        .Name = "X-RateLimit-Remaining",
        .Value = "4999",
    });
    result.push_back({
        .Name = "X-RateLimit-Reset",
        .Value = "2000000000",
    });
    result.push_back({
        .Name = "X-RateLimit-Resource",
        .Value = "core",
    });
    return result;
}

[[nodiscard]] auto success_response(
    std::string body,
    std::optional<std::string> link = std::nullopt,
    const page_completeness completeness = page_completeness::complete)
    -> wire_response
{
    return {
        .Status = 200,
        .Headers = response_headers(std::move(link)),
        .Body = std::move(body),
        .Completeness = completeness,
    };
}

[[nodiscard]] auto profile() -> validated_github_host_profile
{
    const auto result =
        validate_github_host_profile(github_host_profile_candidate{
            .Profile = core::host_profile_id{11U},
            .Host = core::git_host_id{22U},
            .Revision = application::github_profile_revision{3U},
            .Server =
                application::github_server_contract::github_com_2022_11_28,
            .Authentication =
                application::github_authentication_transport::native_https,
            .Proxy = application::github_proxy_mode::system,
            .Trust = application::github_trust_mode::system,
            .Provenance = github_profile_provenance::explicit_user,
            .Hostname = "github.com",
            .RestRoot = "https://api.github.com",
            .GraphqlRoot = "https://api.github.com/graphql",
        });
    expect(result.has_value(), "test profile validates");
    return *result;
}

[[nodiscard]] auto enterprise_profile() -> validated_github_host_profile
{
    const auto result =
        validate_github_host_profile(github_host_profile_candidate{
            .Profile = core::host_profile_id{11U},
            .Host = core::git_host_id{22U},
            .Revision = application::github_profile_revision{3U},
            .Server = application::github_server_contract::ghes_3_18_2022_11_28,
            .Authentication =
                application::github_authentication_transport::native_https,
            .Proxy = application::github_proxy_mode::system,
            .Trust = application::github_trust_mode::system,
            .Provenance = github_profile_provenance::explicit_user,
            .Hostname = "ghe.example.invalid",
            .RestRoot = "https://ghe.example.invalid/api/v3",
            .GraphqlRoot = "https://ghe.example.invalid/api/graphql",
        });
    expect(result.has_value(), "enterprise test profile validates");
    return *result;
}

class test_account_transport final
    : public application::github_account_transport_lease
{
};

[[nodiscard]] auto
provider_request(const application::github_server_contract server =
                     application::github_server_contract::github_com_2022_11_28)
    -> application::github_pull_request_provider_request
{
    const application::github_capability_authority_key authority{
        .Profile = core::host_profile_id{11U},
        .ProfileRevision = application::github_profile_revision{3U},
        .Host = core::git_host_id{22U},
        .Account = core::github_account_id{1001U},
        .ServerContract = server,
        .ProbeContractRevision =
            application::github_probe_contract_revision{1U},
    };
    std::array<application::github_capability_fact,
               application::github_capability_fact_count>
        aggregate{};
    const std::array aggregate_states{
        core::github_capability_state::supported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
    };
    for(std::size_t index = 0U; index < aggregate.size(); ++index)
    {
        aggregate[index] = {
            .Capability = application::canonical_github_capabilities[index],
            .State = aggregate_states[index],
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }
    std::array<application::github_schema_capability_fact,
               application::github_schema_capability_fact_count>
        schema{};
    const std::array schema_states{
        core::github_capability_state::supported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
        core::github_capability_state::unsupported,
        core::github_capability_state::supported,
        core::github_capability_state::unsupported,
    };
    for(std::size_t index = 0U; index < schema.size(); ++index)
    {
        schema[index] = {
            .Capability =
                application::canonical_github_schema_capabilities[index],
            .State = schema_states[index],
            .Source = application::github_capability_evidence_source::
                observed_contract,
        };
    }

    return {
        .Context =
            {
                .Request = core::request_id{41U},
                .ApplicationSession = core::application_session_id{7U},
                .Profile = core::host_profile_id{11U},
                .Host = core::git_host_id{22U},
                .Account = core::github_account_id{1001U},
                .Repository =
                    {
                        .Owner = "octo-org",
                        .Name = "review-repository",
                    },
                .Number = core::pull_request_number{17U},
                .ExpectedRepository = std::nullopt,
                .ExpectedPullRequest = std::nullopt,
                .Capabilities = core::capability_revision{44U},
                .IdentityGeneration = core::github_identity_generation{3U},
                .Generation = core::request_generation{9U},
            },
        .ProfileAuthority =
            {
                .Profile = core::host_profile_id{11U},
                .ProfileRevision = application::github_profile_revision{3U},
                .Host = core::git_host_id{22U},
                .ServerContract = server,
                .Authentication =
                    application::github_authentication_transport::native_https,
                .ProbeContractRevision =
                    application::github_probe_contract_revision{1U},
            },
        .Viewer =
            {
                .Account = core::github_account_id{1001U},
                .Node = application::github_actor_id{"U_viewer"},
                .Login = "octo-reviewer",
                .DisplayName = "Octo Reviewer",
            },
        .CapabilityEvidence =
            {
                .Authority = authority,
                .Facts = aggregate,
                .SchemaFacts = schema,
                .Capabilities =
                    {
                        .ReviewThreads =
                            core::github_capability_state::supported,
                        .ViewedFileMutations =
                            core::github_capability_state::unsupported,
                        .ResolveThread =
                            core::github_capability_state::unsupported,
                        .UnresolveThread =
                            core::github_capability_state::unsupported,
                    },
                .ReadShape =
                    {
                        .ReviewThreads = true,
                        .ViewedFileState = false,
                        .ResolvePermission = false,
                        .UnresolvePermission = true,
                    },
                .Revision = core::capability_revision{44U},
                .ObservedAt = application::github_freshness_epoch{100U},
                .ExpiresBefore = application::github_freshness_epoch{400U},
            },
        .AccountTransport = std::make_shared<test_account_transport>(),
        .Attempt = application::github_provider_attempt{1U},
    };
}

[[nodiscard]] auto rest_actor(const std::string_view node,
                              const std::string_view login) -> QJsonObject
{
    return {
        {"avatar_url", "https://avatars.githubusercontent.com/u/123"},
        {"id", 123},
        {"login", QString::fromUtf8(login)},
        {"node_id", QString::fromUtf8(node)},
        {"type", "User"},
    };
}

[[nodiscard]] auto comment_node(const std::size_t index) -> QJsonObject
{
    return {
        {"author", QJsonValue::Null},
        {"bodyText", QString{"comment %1"}.arg(index)},
        {"id", QString{"C_%1"}.arg(index)},
        {"line", 42},
        {"originalLine", QJsonValue::Null},
        {"outdated", false},
        {"path", "src/file0.cpp"},
        {"replyTo", QJsonValue::Null},
    };
}

[[nodiscard]] auto page_info(const bool has_next,
                             const std::optional<std::string_view> cursor)
    -> QJsonObject
{
    return {
        {
            "endCursor",
            cursor.has_value() ? QJsonValue{QString::fromUtf8(*cursor)}
                               : QJsonValue{QJsonValue::Null},
        },
        {"hasNextPage", has_next},
    };
}

[[nodiscard]] auto connection(const std::uint64_t total,
                              QJsonArray nodes,
                              const bool has_next,
                              const std::optional<std::string_view> cursor)
    -> QJsonObject
{
    return {
        {"nodes", std::move(nodes)},
        {"pageInfo", page_info(has_next, cursor)},
        {"totalCount", static_cast<qint64>(total)},
    };
}

enum class fixture_scenario : std::uint8_t
{
    success,
    raw_object_ceiling_exceeded,
    raw_depth_ceiling_exceeded,
    aggregate_byte_ceiling,
    large_ignored_patch,
    selected_title_over_limit,
    selected_page_records_exceeded,
    page_first_query,
    numeric_repository_link,
    cross_authority_link,
    wrong_numeric_repository_link,
    path_drift_link,
    fragment_link,
    userinfo_link,
    port_link,
    duplicate_link_query,
    extra_link_query,
    encoded_link_query,
    search_plus_link,
    search_query_semantic_drift,
    search_query_double_encoded,
    search_query_double_plus,
    unknown_completeness,
    missing_cursor,
    repeated_cursor,
    truncated_total,
    graphql_extensions,
    graphql_errors_type_rate_limited,
    graphql_partial_errors_extension_type,
    graphql_partial_errors_extension_code,
    graphql_primary_rate_metadata,
    graphql_malformed_errors,
    graphql_unknown_envelope_field,
    rest_secondary_no_retry,
    rest_secondary_with_retry,
    rest_secondary_primary_metadata,
    rest_secondary_duplicate_message,
    rest_generic_403,
    rest_429,
    rest_408,
    rest_503_retry,
    rest_504,
    network_failure,
    wrong_resource_wire_error,
    late_search_wire_failure,
    repository_missing_required,
    repository_wrong_required,
    pull_request_missing_required,
    pull_request_wrong_required,
    actor_missing_required,
    actor_wrong_required,
    file_missing_required,
    file_wrong_required,
    commit_missing_required,
    commit_wrong_required,
    review_missing_required,
    review_wrong_required,
    issue_comment_missing_required,
    issue_comment_wrong_required,
    check_missing_required,
    check_wrong_required,
    check_envelope_missing_required,
    check_envelope_wrong_required,
    search_missing_required,
    search_wrong_required,
    search_wrong_authority,
    search_wrong_owner,
    search_wrong_repository,
    search_wrong_number_url,
    search_missing_pull_request,
    search_wrong_pull_request_url,
    search_canonical_repository_casing,
    search_envelope_missing_required,
    search_envelope_wrong_required,
    pull_request_missing_file_total,
    pull_request_wrong_file_total,
    pull_request_missing_commit_total,
    pull_request_wrong_commit_total,
    file_count_mismatch,
    commit_count_mismatch,
    file_count_above_cap,
    commit_count_above_cap,
    collection_cap_exact,
};

class paged_fixture_transport final : public github_wire_transport_port
{
public:
    explicit paged_fixture_transport(const fixture_scenario scenario,
                                     std::string authority = "api.github.com")
        : Scenario{scenario}
        , Authority{std::move(authority)}
    {
    }

    [[nodiscard]] auto perform(const github_wire_request& request,
                               std::stop_token)
        -> github_wire_completion override
    {
        Requests.push_back(request);
        if(Scenario == fixture_scenario::network_failure
           && Requests.size() == 1U)
        {
            return github_wire_error{
                .Correlation = request.Correlation,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::network,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits = {},
                        .RateLimitCause = std::nullopt,
                    },
            };
        }
        if(Scenario == fixture_scenario::wrong_resource_wire_error
           && Requests.size() == 1U)
        {
            return github_wire_error{
                .Correlation = request.Correlation,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::network,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits =
                            {
                                .Core = std::nullopt,
                                .Search =
                                    application::github_rate_limit_record{
                                        .Resource = application::
                                            github_rate_limit_resource::search,
                                        .Limit = 30U,
                                        .Remaining = 29U,
                                        .ResetUnixSeconds = 2'000'000'000U,
                                    },
                                .Graphql = std::nullopt,
                            },
                        .RateLimitCause = std::nullopt,
                    },
            };
        }
        if(Scenario == fixture_scenario::late_search_wire_failure
           && request.Wire.Operation == read_operation::search_pull_requests)
        {
            return github_wire_error{
                .Correlation = request.Correlation,
                .Error =
                    {
                        .Failure =
                            application::github_provider_failure::network,
                        .RetryAfterSeconds = std::nullopt,
                        .RateLimits =
                            {
                                .Core = std::nullopt,
                                .Search =
                                    application::github_rate_limit_record{
                                        .Resource = application::
                                            github_rate_limit_resource::search,
                                        .Limit = 30U,
                                        .Remaining = 17U,
                                        .ResetUnixSeconds = 2'000'000'001U,
                                    },
                                .Graphql = std::nullopt,
                            },
                        .RateLimitCause = std::nullopt,
                    },
            };
        }
        return github_wire_success{
            .Correlation = request.Correlation,
            .Response = response_for(request),
        };
    }

    std::vector<github_wire_request> Requests;

private:
    [[nodiscard]] auto advertised_file_count() const noexcept -> std::uint32_t
    {
        if(Scenario == fixture_scenario::collection_cap_exact)
        {
            return 3'000U;
        }
        if(Scenario == fixture_scenario::file_count_mismatch)
        {
            return 102U;
        }
        if(Scenario == fixture_scenario::file_count_above_cap)
        {
            return 3'001U;
        }
        return 101U;
    }

    [[nodiscard]] auto advertised_commit_count() const noexcept -> std::uint32_t
    {
        if(Scenario == fixture_scenario::collection_cap_exact)
        {
            return 250U;
        }
        if(Scenario == fixture_scenario::commit_count_mismatch)
        {
            return 2U;
        }
        if(Scenario == fixture_scenario::commit_count_above_cap)
        {
            return 251U;
        }
        return 1U;
    }

    [[nodiscard]] auto repository_response() const -> wire_response
    {
        int operational_status = 0;
        std::string operational_message;
        std::optional<std::string> retry_after;
        switch(Scenario)
        {
        case fixture_scenario::rest_secondary_no_retry:
            operational_status = 403;
            operational_message =
                "You have exceeded a SECONDARY RATE LIMIT. Please wait.";
            break;
        case fixture_scenario::rest_secondary_with_retry:
            operational_status = 403;
            operational_message = "secondary rate limit";
            retry_after = "17";
            break;
        case fixture_scenario::rest_secondary_primary_metadata:
            operational_status = 403;
            operational_message = "secondary rate limit";
            break;
        case fixture_scenario::rest_secondary_duplicate_message:
            operational_status = 403;
            break;
        case fixture_scenario::rest_generic_403:
            operational_status = 403;
            operational_message = "API rate limit policy information";
            break;
        case fixture_scenario::rest_429:
            operational_status = 429;
            operational_message = "Too many requests";
            break;
        case fixture_scenario::rest_408:
            operational_status = 408;
            operational_message = "Request timed out";
            break;
        case fixture_scenario::rest_503_retry:
            operational_status = 503;
            operational_message = "Temporarily unavailable";
            retry_after = "9";
            break;
        case fixture_scenario::rest_504:
            operational_status = 504;
            operational_message = "Gateway timeout";
            break;
        default:
            break;
        }
        if(operational_status != 0)
        {
            auto response = success_response(
                Scenario == fixture_scenario::rest_secondary_duplicate_message
                    ? R"({"message":"secondary rate limit","message":"permission"})"
                    : json(QJsonObject{
                          {"message",
                           QString::fromStdString(operational_message)},
                      }));
            response.Status = operational_status;
            if(retry_after.has_value())
            {
                response.Headers.push_back({
                    .Name = "Retry-After",
                    .Value = *retry_after,
                });
            }
            if(Scenario == fixture_scenario::rest_secondary_primary_metadata)
            {
                for(auto& header : response.Headers)
                {
                    if(header.Name == "X-RateLimit-Remaining")
                    {
                        header.Value = "0";
                    }
                }
            }
            return response;
        }
        if(Scenario == fixture_scenario::raw_depth_ceiling_exceeded)
        {
            QJsonValue nested = QJsonValue::Null;
            for(std::size_t depth = 0U; depth < 33U; ++depth)
            {
                nested = QJsonObject{{"nested", nested}};
            }
            return success_response(json(nested.toObject()));
        }
        const auto canonical_casing =
            Scenario == fixture_scenario::search_canonical_repository_casing;
        const auto owner_name = canonical_casing ? std::string{"Octo-Org"}
                                                 : std::string{"octo-org"};
        const auto repository_name = canonical_casing
                                         ? std::string{"Review-Repository"}
                                         : std::string{"review-repository"};
        auto repository = QJsonObject{
            {"archive_url",
             "https://api.github.com/repos/octo-org/review-repository/"
             "{archive_format}{/ref}"},
            {"default_branch", "main"},
            {"full_name",
             QString::fromStdString(owner_name + "/" + repository_name)},
            {"html_url", "https://github.com/octo-org/review-repository"},
            {"id", 2001},
            {"name", QString::fromStdString(repository_name)},
            {"node_id", "R_repository"},
            {"owner", rest_actor("O_organization", owner_name)},
            {"private", false},
            {"visibility", "private"},
        };
        QJsonArray ignored_topics;
        for(std::size_t index = 0U; index < 101U; ++index)
        {
            ignored_topics.push_back(QString{"topic-%1"}.arg(index));
        }
        repository.insert("ignored_topics", ignored_topics);
        if(Scenario == fixture_scenario::aggregate_byte_ceiling)
        {
            repository.insert("aggregate_padding",
                              QString{70'000U, QLatin1Char{'r'}});
        }
        if(Scenario == fixture_scenario::repository_missing_required)
        {
            repository.remove("default_branch");
        }
        else if(Scenario == fixture_scenario::repository_wrong_required)
        {
            repository.insert("id", "not-an-integer");
        }
        fill_to_member_count(
            repository,
            "repository_surplus",
            Scenario == fixture_scenario::raw_object_ceiling_exceeded ? 257
                                                                      : 86);
        return success_response(json(repository));
    }

    [[nodiscard]] auto pull_request_response() const -> wire_response
    {
        auto base_repository = QJsonObject{
            {"full_name", "octo-org/review-repository"},
            {"html_url", "https://github.com/octo-org/review-repository"},
            {"id", 2001},
            {"node_id", "R_repository"},
        };
        fill_to_member_count(base_repository, "base_repo_surplus", 81);
        auto head_repository = base_repository;
        fill_to_member_count(head_repository, "head_repo_surplus", 81);
        const auto base = QJsonObject{
            {"label", "octo-org:main"},
            {"ref", "main"},
            {"repo", base_repository},
            {"sha", QString::fromUtf8(base_oid)},
            {"user", rest_actor("O_organization", "octo-org")},
        };
        const auto head = QJsonObject{
            {"label", "octo-org:feature/paged-read"},
            {"ref", "feature/paged-read"},
            {"repo", head_repository},
            {"sha", QString::fromUtf8(head_oid)},
            {"user", rest_actor("O_organization", "octo-org")},
        };
        auto author = rest_actor("U_author", "octo-author");
        if(Scenario == fixture_scenario::actor_missing_required)
        {
            author.remove("node_id");
        }
        else if(Scenario == fixture_scenario::actor_wrong_required)
        {
            author.insert("login", 123);
        }
        auto pull_request = QJsonObject{
            {"base", base},
            {"body", "Synthetic body"},
            {"changed_files", static_cast<qint64>(advertised_file_count())},
            {"comments", 1},
            {"commits", static_cast<qint64>(advertised_commit_count())},
            {"created_at", "2026-07-30T12:00:00Z"},
            {"draft", false},
            {"head", head},
            {"html_url",
             "https://github.com/octo-org/review-repository/pull/17"},
            {"id", 3001},
            {"locked", false},
            {"merged", false},
            {"node_id", "PR_17"},
            {"number", 17},
            {"state", "open"},
            {"title", "Exhaust every page"},
            {"updated_at", "2026-07-30T12:30:00Z"},
            {"user", author},
        };
        if(Scenario == fixture_scenario::aggregate_byte_ceiling)
        {
            pull_request.insert("aggregate_padding",
                                QString{70'000U, QLatin1Char{'p'}});
        }
        if(Scenario == fixture_scenario::pull_request_missing_required)
        {
            pull_request.remove("title");
        }
        else if(Scenario == fixture_scenario::pull_request_wrong_required)
        {
            pull_request.insert("draft", "false");
        }
        else if(Scenario == fixture_scenario::pull_request_missing_file_total)
        {
            pull_request.remove("changed_files");
        }
        else if(Scenario == fixture_scenario::pull_request_wrong_file_total)
        {
            pull_request.insert("changed_files", "101");
        }
        else if(Scenario == fixture_scenario::pull_request_missing_commit_total)
        {
            pull_request.remove("commits");
        }
        else if(Scenario == fixture_scenario::pull_request_wrong_commit_total)
        {
            pull_request.insert("commits", "1");
        }
        else if(Scenario == fixture_scenario::selected_title_over_limit)
        {
            pull_request.insert("title", QString{1'025U, QLatin1Char{'t'}});
        }
        return success_response(json(pull_request));
    }

    [[nodiscard]] auto files_response(const github_wire_request& request) const
        -> wire_response
    {
        QJsonArray files;
        const auto page = request.Wire.Semantic.Page;
        const auto total = Scenario == fixture_scenario::collection_cap_exact
                               ? std::size_t{3'000U}
                               : std::size_t{101U};
        const auto begin =
            (static_cast<std::size_t>(page) - 1U) * std::size_t{100U};
        const auto end =
            Scenario == fixture_scenario::selected_page_records_exceeded
                    && page == 1U
                ? total
                : std::min(begin + std::size_t{100U}, total);
        for(auto index = begin; index < end; ++index)
        {
            auto file = QJsonObject{
                {"additions", 1},
                {"blob_url",
                 QString{"https://github.com/octo-org/review-repository/"
                         "blob/head/src/file%1.cpp"}
                     .arg(index)},
                {"changes", 1},
                {"contents_url",
                 QString{"https://api.github.com/repos/octo-org/"
                         "review-repository/contents/src/file%1.cpp"}
                     .arg(index)},
                {"deletions", 0},
                {
                    "filename",
                    QString{"src/file%1.cpp"}.arg(index),
                },
                {"patch", "@@ -1 +1 @@"},
                {"raw_url",
                 QString{"https://github.com/octo-org/review-repository/"
                         "raw/head/src/file%1.cpp"}
                     .arg(index)},
                {"sha", QString::fromUtf8(head_oid)},
                {"status", "modified"},
            };
            if(index == 0U
               && Scenario == fixture_scenario::file_missing_required)
            {
                file.remove("filename");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::file_wrong_required)
            {
                file.insert("additions", "one");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::large_ignored_patch)
            {
                file.insert("patch", QString{500U * 1'024U, QLatin1Char{'p'}});
            }
            files.push_back(std::move(file));
        }
        const auto has_next = end < total;
        if(!has_next)
        {
            const auto completeness =
                Scenario == fixture_scenario::unknown_completeness
                    ? page_completeness::unknown
                    : page_completeness::complete;
            return success_response(json(files), std::nullopt, completeness);
        }
        auto authority = "https://" + Authority;
        if(Scenario == fixture_scenario::cross_authority_link)
        {
            authority = "https://evil.example.invalid";
        }
        else if(Scenario == fixture_scenario::userinfo_link)
        {
            authority = "https://octo@api.github.com";
        }
        else if(Scenario == fixture_scenario::port_link)
        {
            authority = "https://api.github.com:443";
        }
        auto path = request.Wire.Path;
        if(Scenario == fixture_scenario::numeric_repository_link
           || Scenario == fixture_scenario::wrong_numeric_repository_link)
        {
            const auto named = std::string{"/repos/octo-org/review-repository"};
            const auto position = path.find(named);
            expect(position != std::string::npos,
                   "fixture canonical repository path is present");
            const auto id =
                Scenario == fixture_scenario::numeric_repository_link ? "2001"
                                                                      : "9999";
            path.replace(
                position, named.size(), std::string{"/repositories/"} + id);
        }
        else if(Scenario == fixture_scenario::path_drift_link)
        {
            path += "/drift";
        }
        auto query = Scenario == fixture_scenario::page_first_query
                         ? "page=" + std::to_string(page + 1U) + "&per_page=100"
                         : "per_page=100&page=" + std::to_string(page + 1U);
        if(Scenario == fixture_scenario::duplicate_link_query)
        {
            query += "&page=" + std::to_string(page + 1U);
        }
        else if(Scenario == fixture_scenario::extra_link_query)
        {
            query += "&unexpected=1";
        }
        else if(Scenario == fixture_scenario::encoded_link_query)
        {
            query = "%70age=" + std::to_string(page + 1U) + "&per_page=100";
        }
        if(Scenario == fixture_scenario::fragment_link)
        {
            query += "#fragment";
        }
        const auto link =
            "<" + authority + path + "?" + query + ">; rel=\"next\"";
        return success_response(
            json(files), link, page_completeness::has_next_page);
    }

    [[nodiscard]] auto
    commits_response(const github_wire_request& request) const -> wire_response
    {
        const auto total = Scenario == fixture_scenario::collection_cap_exact
                               ? std::size_t{250U}
                               : std::size_t{1U};
        const auto page = request.Wire.Semantic.Page;
        const auto begin =
            (static_cast<std::size_t>(page) - 1U) * std::size_t{100U};
        const auto end = std::min(begin + std::size_t{100U}, total);
        QJsonArray commits;
        for(auto index = begin; index < end; ++index)
        {
            const auto numeric = std::to_string(index + 1U);
            auto oid = std::string(40U, '0');
            oid.replace(oid.size() - numeric.size(), numeric.size(), numeric);
            if(index + 1U == total)
            {
                oid = head_oid;
            }
            auto commit_details = QJsonObject{
                {
                    "author",
                    QJsonObject{
                        {"date", "2026-07-30T12:00:00Z"},
                        {"email", "author@example.invalid"},
                        {"name", "Octo Author"},
                    },
                },
                {"comment_count", 0},
                {"message",
                 index == 0U ? QString{"Paged read\n\nBody"}
                             : QString{"Paged read %1\n\nBody"}.arg(index)},
                {
                    "tree",
                    QJsonObject{
                        {"sha", QString::fromUtf8(base_oid)},
                        {"url", "https://api.github.com/tree"},
                    },
                },
                {"url", "https://api.github.com/commit"},
            };
            auto commit = QJsonObject{
                {"author", rest_actor("U_commit_author", "octo-commit-author")},
                {"comments_url", "https://api.github.com/commit/comments"},
                {"commit", commit_details},
                {"html_url", "https://github.com/commit"},
                {"node_id", QString{"COMMIT_%1"}.arg(index)},
                {"sha", QString::fromStdString(oid)},
                {"url", "https://api.github.com/commit"},
            };
            if(index == 0U
               && Scenario == fixture_scenario::commit_missing_required)
            {
                commit.remove("commit");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::commit_wrong_required)
            {
                commit.insert("sha", "not-a-commit-oid");
            }
            commits.push_back(std::move(commit));
        }
        if(end == total)
        {
            return success_response(json(commits));
        }
        const auto link = "<https://" + Authority + request.Wire.Path
                          + "?per_page=100&page=" + std::to_string(page + 1U)
                          + ">; rel=\"next\"";
        return success_response(
            json(commits), link, page_completeness::has_next_page);
    }

    [[nodiscard]] auto reviews_response() const -> wire_response
    {
        auto review = QJsonObject{
            {"author_association", "COLLABORATOR"},
            {"body", "Looks good"},
            {"commit_id", QString::fromUtf8(head_oid)},
            {"html_url",
             "https://github.com/octo-org/review-repository/pull/17#review"},
            {"id", 4001},
            {"node_id", "R_review"},
            {"state", "APPROVED"},
            {"submitted_at", "2026-07-30T13:00:00Z"},
            {"user", rest_actor("U_reviewer", "octo-reviewer-2")},
        };
        if(Scenario == fixture_scenario::review_missing_required)
        {
            review.remove("state");
        }
        else if(Scenario == fixture_scenario::review_wrong_required)
        {
            review.insert("body", 42);
        }
        return success_response(json(QJsonArray{review}));
    }

    [[nodiscard]] auto issue_comments_response() const -> wire_response
    {
        auto comment = QJsonObject{
            {"author_association", "MEMBER"},
            {"body", "Tracking comment"},
            {"created_at", "2026-07-30T13:30:00Z"},
            {"html_url",
             "https://github.com/octo-org/review-repository/issues/17#comment"},
            {"id", 5001},
            {"issue_url",
             "https://api.github.com/repos/octo-org/review-repository/"
             "issues/17"},
            {"node_id", "IC_comment"},
            {"updated_at", "2026-07-30T13:30:00Z"},
            {"user", rest_actor("U_commenter", "octo-commenter")},
        };
        if(Scenario == fixture_scenario::issue_comment_missing_required)
        {
            comment.remove("body");
        }
        else if(Scenario == fixture_scenario::issue_comment_wrong_required)
        {
            comment.insert("user", "not-an-actor");
        }
        return success_response(json(QJsonArray{comment}));
    }

    [[nodiscard]] auto checks_response() const -> wire_response
    {
        auto check = QJsonObject{
            {"completed_at", "2026-07-30T13:45:00Z"},
            {"conclusion", "success"},
            {"details_url", "https://checks.example.invalid/6001"},
            {"head_sha", QString::fromUtf8(head_oid)},
            {"html_url", "https://api.github.com/check-runs/6001"},
            {"id", 6001},
            {"name", "native-build"},
            {"node_id", "CHK_native"},
            {"started_at", "2026-07-30T13:40:00Z"},
            {"status", "completed"},
        };
        if(Scenario == fixture_scenario::check_missing_required)
        {
            check.remove("status");
        }
        else if(Scenario == fixture_scenario::check_wrong_required)
        {
            check.insert("head_sha", QString::fromUtf8(base_oid));
        }
        auto envelope = QJsonObject{
            {"check_runs", QJsonArray{check}},
            {"total_count", 1},
            {"url",
             "https://api.github.com/repos/octo-org/review-repository/"
             "commits/head/check-runs"},
        };
        if(Scenario == fixture_scenario::check_envelope_missing_required)
        {
            envelope.remove("total_count");
        }
        else if(Scenario == fixture_scenario::check_envelope_wrong_required)
        {
            envelope.insert("check_runs", "not-an-array");
        }
        return success_response(json(envelope));
    }

    [[nodiscard]] auto search_response(const github_wire_request& request) const
        -> wire_response
    {
        const auto paged =
            Scenario == fixture_scenario::search_plus_link
            || Scenario == fixture_scenario::search_query_semantic_drift
            || Scenario == fixture_scenario::search_query_double_encoded
            || Scenario == fixture_scenario::search_query_double_plus;
        const auto total = paged ? std::size_t{101U} : std::size_t{1U};
        const auto page = request.Wire.Semantic.Page;
        const auto begin =
            (static_cast<std::size_t>(page) - 1U) * std::size_t{100U};
        const auto end = std::min(begin + std::size_t{100U}, total);
        const auto canonical_casing =
            Scenario == fixture_scenario::search_canonical_repository_casing;
        const auto owner = canonical_casing ? std::string{"Octo-Org"}
                                            : std::string{"octo-org"};
        const auto repository = canonical_casing
                                    ? std::string{"Review-Repository"}
                                    : std::string{"review-repository"};
        const auto repository_url =
            Authority == "api.github.com"
                ? "https://api.github.com/repos/" + owner + "/" + repository
                : "https://" + Authority + "/api/v3/repos/" + owner + "/"
                      + repository;
        const auto web_authority = Authority == "api.github.com"
                                       ? std::string{"github.com"}
                                       : Authority;
        QJsonArray items;
        for(auto index = begin; index < end; ++index)
        {
            const auto number = 18U + index;
            auto item = QJsonObject{
                {"comments", 0},
                {"draft", false},
                {"html_url",
                 QString::fromStdString("https://" + web_authority + "/" + owner
                                        + "/" + repository + "/pull/"
                                        + std::to_string(number))},
                {"id", static_cast<qint64>(7'001U + index)},
                {"node_id", QString{"PR_%1"}.arg(number)},
                {"number", static_cast<qint64>(number)},
                {
                    "pull_request",
                    QJsonObject{
                        {"diff_url", "https://github.com/pull.diff"},
                        {"html_url", "https://github.com/pull"},
                        {"patch_url", "https://github.com/pull.patch"},
                        {"url",
                         QString::fromStdString(repository_url + "/pulls/"
                                                + std::to_string(number))},
                    },
                },
                {"repository_url", QString::fromStdString(repository_url)},
                {"state", "open"},
                {"title", QString{"Open pull request %1"}.arg(number)},
                {"url", "https://api.github.com/issues"},
            };
            if(index == 0U
               && Scenario == fixture_scenario::search_missing_required)
            {
                item.remove("number");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::search_wrong_required)
            {
                item.insert("draft", "false");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::search_wrong_authority)
            {
                item.insert("repository_url",
                            "https://attacker.invalid/repos/octo-org/"
                            "review-repository");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::search_wrong_owner)
            {
                item.insert(
                    "repository_url",
                    "https://api.github.com/repos/other/review-repository");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::search_wrong_repository)
            {
                item.insert("repository_url",
                            "https://api.github.com/repos/octo-org/other");
            }
            else if(index == 0U
                    && Scenario == fixture_scenario::search_wrong_number_url)
            {
                item.insert(
                    "html_url",
                    "https://github.com/octo-org/review-repository/pull/999");
            }
            else if(index == 0U
                    && Scenario
                           == fixture_scenario::search_missing_pull_request)
            {
                item.remove("pull_request");
            }
            else if(index == 0U
                    && Scenario
                           == fixture_scenario::search_wrong_pull_request_url)
            {
                auto pull_request = item.value("pull_request").toObject();
                pull_request.insert("url",
                                    "https://api.github.com/repos/octo-org/"
                                    "review-repository/pulls/999");
                item.insert("pull_request", pull_request);
            }
            items.push_back(std::move(item));
        }
        auto envelope = QJsonObject{
            {"incomplete_results", false},
            {"items", items},
            {"repository_selection", "selected"},
            {"total_count", static_cast<qint64>(total)},
        };
        if(Scenario == fixture_scenario::search_envelope_missing_required)
        {
            envelope.remove("incomplete_results");
        }
        else if(Scenario == fixture_scenario::search_envelope_wrong_required)
        {
            envelope.insert("items", QJsonObject{});
        }
        if(end == total)
        {
            return success_response(json(envelope));
        }

        const auto marker = request.Wire.Query.find("&q=");
        expect(marker != std::string::npos,
               "search fixture has a canonical q parameter");
        auto query_value = request.Wire.Query.substr(marker + 3U);
        std::size_t space = 0U;
        while((space = query_value.find("%20", space)) != std::string::npos)
        {
            query_value.replace(space, 3U, "+");
            ++space;
        }
        if(Scenario == fixture_scenario::search_query_semantic_drift)
        {
            const auto state = query_value.find("is%3Aopen");
            expect(state != std::string::npos,
                   "search fixture has canonical open state");
            query_value.replace(
                state, std::string_view{"is%3Aopen"}.size(), "is%3Aclosed");
        }
        else if(Scenario == fixture_scenario::search_query_double_encoded)
        {
            const auto colon = query_value.find("%3A");
            expect(colon != std::string::npos,
                   "search fixture has canonical encoded colon");
            query_value.replace(colon, 3U, "%253A");
        }
        else if(Scenario == fixture_scenario::search_query_double_plus)
        {
            const auto plus = query_value.find('+');
            expect(plus != std::string::npos,
                   "search fixture has canonical form space");
            query_value.insert(plus, "+");
        }
        const auto link = "<https://" + Authority + request.Wire.Path
                          + "?per_page=100&page=" + std::to_string(page + 1U)
                          + "&q=" + query_value + ">; rel=\"next\"";
        return success_response(
            json(envelope), link, page_completeness::has_next_page);
    }

    [[nodiscard]] auto graphql_initial_response() const -> wire_response
    {
        std::uint64_t total = 101U;
        if(Scenario == fixture_scenario::repeated_cursor)
        {
            total = 201U;
        }
        else if(Scenario == fixture_scenario::truncated_total)
        {
            total = 102U;
        }
        QJsonArray comments;
        for(std::size_t index = 0U; index < 100U; ++index)
        {
            comments.push_back(comment_node(index));
        }
        const auto missing = Scenario == fixture_scenario::missing_cursor;
        const auto thread = QJsonObject{
            {
                "comments",
                connection(total,
                           std::move(comments),
                           true,
                           missing
                               ? std::nullopt
                               : std::optional<std::string_view>{"CURSOR_1"}),
            },
            {"id", "T_1"},
            {"isOutdated", false},
            {"isResolved", false},
            {"line", 42},
            {"originalLine", QJsonValue::Null},
            {"path", "src/file0.cpp"},
            {"subjectType", "LINE"},
            {"viewerCanUnresolve", true},
        };
        const auto pull_request = QJsonObject{
            {"id", "PR_17"},
            {
                "reviewThreads",
                connection(1U,
                           QJsonArray{thread},
                           false,
                           std::optional<std::string_view>{"THREAD_END"}),
            },
        };
        auto envelope = QJsonObject{
            {
                "data",
                QJsonObject{
                    {
                        "repository",
                        QJsonObject{
                            {"id", "R_repository"},
                            {"pullRequest", pull_request},
                        },
                    },
                },
            },
        };
        if(Scenario == fixture_scenario::graphql_extensions)
        {
            envelope.insert(
                "extensions",
                QJsonObject{{"requestId", "graphql-initial-request"}});
        }
        else if(Scenario == fixture_scenario::graphql_errors_type_rate_limited)
        {
            envelope = QJsonObject{
                {
                    "errors",
                    QJsonArray{
                        QJsonObject{
                            {"message", "secondary limit"},
                            {"type", "RATE_LIMITED"},
                        },
                    },
                },
            };
        }
        else if(Scenario
                == fixture_scenario::graphql_partial_errors_extension_type)
        {
            envelope.insert("errors",
                            QJsonArray{
                                QJsonObject{
                                    {
                                        "extensions",
                                        QJsonObject{{"type", "RATE_LIMITED"}},
                                    },
                                    {"message", "secondary limit"},
                                },
                            });
        }
        else if(Scenario
                == fixture_scenario::graphql_partial_errors_extension_code)
        {
            envelope.insert("errors",
                            QJsonArray{
                                QJsonObject{
                                    {
                                        "extensions",
                                        QJsonObject{{"code", "RATE_LIMITED"}},
                                    },
                                    {"message", "secondary limit"},
                                },
                            });
        }
        else if(Scenario == fixture_scenario::graphql_primary_rate_metadata)
        {
            envelope.insert("errors",
                            QJsonArray{
                                QJsonObject{
                                    {"message", "primary quota exhausted"},
                                    {"type", "RATE_LIMITED"},
                                },
                            });
        }
        else if(Scenario == fixture_scenario::graphql_malformed_errors)
        {
            envelope.insert("errors", QJsonObject{{"type", "RATE_LIMITED"}});
        }
        else if(Scenario == fixture_scenario::graphql_unknown_envelope_field)
        {
            envelope.insert("trace", "not-selected");
        }
        auto response = success_response(json(envelope));
        if(Scenario == fixture_scenario::graphql_primary_rate_metadata)
        {
            for(auto& header : response.Headers)
            {
                if(header.Name == "X-RateLimit-Remaining")
                {
                    header.Value = "0";
                }
            }
        }
        return response;
    }

    [[nodiscard]] auto graphql_comments_response() const -> wire_response
    {
        const auto repeat = Scenario == fixture_scenario::repeated_cursor;
        QJsonArray comments;
        const auto count = repeat ? 100U : 1U;
        for(std::size_t index = 0U; index < count; ++index)
        {
            comments.push_back(comment_node(100U + index));
        }
        const auto total =
            repeat
                ? 201U
                : (Scenario == fixture_scenario::truncated_total ? 102U : 101U);
        const auto comment_connection =
            connection(total,
                       std::move(comments),
                       repeat,
                       repeat ? std::optional<std::string_view>{"CURSOR_1"}
                              : std::optional<std::string_view>{"COMMENT_END"});
        auto envelope = QJsonObject{
            {
                "data",
                QJsonObject{
                    {
                        "node",
                        QJsonObject{
                            {"comments", comment_connection},
                            {"id", "T_1"},
                        },
                    },
                    {
                        "repository",
                        QJsonObject{
                            {"id", "R_repository"},
                            {
                                "pullRequest",
                                QJsonObject{{"id", "PR_17"}},
                            },
                        },
                    },
                },
            },
        };
        if(Scenario == fixture_scenario::graphql_extensions)
        {
            envelope.insert(
                "extensions",
                QJsonObject{{"requestId", "graphql-comments-request"}});
        }
        return success_response(json(envelope));
    }

    [[nodiscard]] auto response_for(const github_wire_request& request) const
        -> wire_response
    {
        auto response = [&]() -> wire_response
        {
            switch(request.Wire.Operation)
            {
            case read_operation::get_repository:
                return repository_response();
            case read_operation::get_pull_request:
                return pull_request_response();
            case read_operation::list_pull_request_files:
                return files_response(request);
            case read_operation::list_pull_request_commits:
                return commits_response(request);
            case read_operation::list_reviews:
                return reviews_response();
            case read_operation::list_issue_comments:
                return issue_comments_response();
            case read_operation::list_review_threads:
                return request.Wire.Semantic.GraphqlPage->Kind
                               == github_graphql_page_kind::initial
                           ? graphql_initial_response()
                           : graphql_comments_response();
            case read_operation::list_checks:
                return checks_response();
            case read_operation::search_pull_requests:
                return search_response(request);
            case read_operation::get_viewer_identity:
                break;
            }
            return success_response(json(QJsonObject{}));
        }();
        const auto resource =
            request.Wire.Operation == read_operation::list_review_threads
                ? "graphql"
                : (request.Wire.Operation
                           == read_operation::search_pull_requests
                       ? "search"
                       : "core");
        for(auto& header : response.Headers)
        {
            if(header.Name == "X-RateLimit-Resource")
            {
                header.Value = resource;
            }
        }
        return response;
    }

    fixture_scenario Scenario = fixture_scenario::success;
    std::string Authority;
};

[[nodiscard]] auto provider_failure_of(const github_paged_read_result& result)
    -> std::optional<application::github_provider_failure>
{
    if(result.has_value())
    {
        return std::nullopt;
    }
    const auto* error =
        std::get_if<application::github_provider_error>(&result.error());
    return error == nullptr ? std::nullopt : std::optional{error->Failure};
}

[[nodiscard]] auto provider_error_of(const github_paged_read_result& result)
    -> std::optional<application::github_provider_error>
{
    if(result.has_value())
    {
        return std::nullopt;
    }
    const auto* error =
        std::get_if<application::github_provider_error>(&result.error());
    return error == nullptr ? std::nullopt : std::optional{*error};
}

auto test_metadata_and_viewer() -> void
{
    const auto response = success_response(json(QJsonObject{
        {"avatar_url", "https://avatars.githubusercontent.com/u/1001"},
        {"bio", "Reviews pull requests"},
        {"created_at", "2011-01-25T18:44:36Z"},
        {"email", QJsonValue::Null},
        {"followers", 20},
        {"following", 0},
        {"html_url", "https://github.com/octo-reviewer"},
        {"id", 1001},
        {"login", "octo-reviewer"},
        {"name", "Octo Reviewer"},
        {"node_id", "U_viewer"},
        {"public_repos", 2},
        {"type", "User"},
    }));
    const auto metadata = parse_github_response_metadata(response);
    expect(metadata.has_value() && metadata->RateLimit.has_value()
               && metadata->RateLimit->Resource
                      == application::github_rate_limit_resource::core
               && metadata->RateLimit->Limit == 5000U
               && metadata->RateLimit->Remaining == 4999U
               && metadata->RateLimit->ResetUnixSeconds == 2000000000U
               && !metadata->Link.has_value(),
           "strict metadata derives exact rate facts");
    const auto viewer = normalize_observed_github_viewer(response);
    expect(viewer.has_value()
               && viewer->Viewer.Account == core::github_account_id{1001U}
               && viewer->Viewer.Node
                      == application::github_actor_id{"U_viewer"}
               && viewer->Viewer.Login == "octo-reviewer"
               && viewer->Viewer.DisplayName == "Octo Reviewer"
               && viewer->RateLimits.Core == metadata->RateLimit,
           "viewer identity and account derive only from raw response");

    auto duplicate = response;
    duplicate.Headers.push_back({
        .Name = "x-ratelimit-limit",
        .Value = "5000",
    });
    expect(!parse_github_response_metadata(duplicate).has_value(),
           "case-folded duplicate metadata is rejected");

    auto unknown = response;
    unknown.Headers.push_back({
        .Name = "Server",
        .Value = "not-allowlisted",
    });
    expect(!parse_github_response_metadata(unknown).has_value(),
           "unknown response metadata is rejected");

    auto invalid_only = wire_response{
        .Status = 200,
        .Headers =
            {
                {
                    .Name = "Content-Type",
                    .Value = "application/json",
                },
                {
                    .Name = "X-RateLimit-Limit",
                    .Value = "not-a-number",
                },
            },
        .Body = "{}",
        .Completeness = page_completeness::complete,
    };
    expect(!parse_github_response_metadata(invalid_only).has_value(),
           "present invalid-only rate metadata cannot collapse to absent");
    invalid_only.Headers[1U] = {
        .Name = "X-RateLimit-Resource",
        .Value = "unknown",
    };
    expect(!parse_github_response_metadata(invalid_only).has_value(),
           "present unknown-only rate resource cannot collapse to absent");
    invalid_only.Headers[1U] = {
        .Name = "Retry-After",
        .Value = "later",
    };
    expect(!parse_github_response_metadata(invalid_only).has_value(),
           "present invalid Retry-After cannot collapse to absent");

    auto enterprise_without_rate = response;
    std::erase_if(enterprise_without_rate.Headers,
                  [](const auto& header)
                  { return header.Name.starts_with("X-RateLimit-"); });
    const auto enterprise_viewer = normalize_observed_github_viewer(
        enterprise_without_rate, server_contract::ghes_3_18);
    expect(enterprise_viewer.has_value()
               && !application::has_observations(enterprise_viewer->RateLimits),
           "GHES may explicitly omit its disabled rate-limit tuple");
    expect(!normalize_observed_github_viewer(enterprise_without_rate,
                                             server_contract::github_com)
                .has_value(),
           "github.com viewer responses require a complete rate-limit tuple");

    auto wrong_resource = response;
    wrong_resource.Headers.back().Value = "search";
    expect(!normalize_observed_github_viewer(wrong_resource).has_value(),
           "viewer response cannot inject a non-core rate-limit bucket");

    auto injected = response;
    injected.Headers.front().Value = "application/json\r\nInjected: yes";
    expect(!parse_github_response_metadata(injected).has_value(),
           "metadata injection is rejected");

    auto missing = response;
    missing.Body = json(QJsonObject{
        {"id", 1001},
        {"login", "octo-reviewer"},
        {"name", "Octo Reviewer"},
    });
    expect(!normalize_observed_github_viewer(missing).has_value(),
           "viewer normalization rejects a missing required field");

    auto wrong = response;
    wrong.Body = json(QJsonObject{
        {"id", "1001"},
        {"login", "octo-reviewer"},
        {"name", "Octo Reviewer"},
        {"node_id", "U_viewer"},
    });
    expect(!normalize_observed_github_viewer(wrong).has_value(),
           "viewer normalization rejects a wrong required field type");

    auto duplicate_json = response;
    duplicate_json.Body =
        R"({"id":1001,"id":1002,"login":"octo-reviewer","name":"Octo Reviewer","node_id":"U_viewer"})";
    expect(!normalize_observed_github_viewer(duplicate_json).has_value(),
           "surplus-field projection retains duplicate-key preflight");

    auto secondary = response;
    secondary.Status = 403;
    secondary.Headers[2].Value = "42";
    secondary.Body =
        R"({"message":"You have exceeded a secondary rate limit. Please wait."})";
    const auto limited = normalize_observed_github_viewer(secondary);
    expect(!limited.has_value()
               && limited.error().Failure
                      == application::github_provider_failure::rate_limit
               && limited.error().RetryAfterSeconds
                      == std::optional<std::uint32_t>{60U}
               && limited.error().RateLimitCause.has_value()
               && limited.error().RateLimitCause->Resource
                      == application::github_rate_limit_resource::core
               && limited.error().RateLimitCause->SecondaryLimited
               && limited.error().RateLimits.Core
                      == limited.error().RateLimitCause,
           "viewer secondary-limit body is typed with bounded fallback");

    secondary.Body =
        R"({"message":"You have exceeded a secondary rate limit.","message":"forged"})";
    const auto duplicate_message = normalize_observed_github_viewer(secondary);
    expect(!duplicate_message.has_value()
               && duplicate_message.error().Failure
                      == application::github_provider_failure::permission
               && !duplicate_message.error().RetryAfterSeconds.has_value(),
           "duplicate viewer error keys cannot claim secondary throttling");
}

auto test_capability_shaped_query() -> void
{
    const auto request = compile_read_request(semantic_read_request{
        .Server = server_contract::github_com,
        .Operation = read_operation::list_review_threads,
        .Owner = "octo-org",
        .Repository = "review-repository",
        .PullRequestNumber = 17U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = 1U,
        .GraphqlPage =
            github_graphql_page_request{
                .Kind = github_graphql_page_kind::initial,
                .Shape =
                    {
                        .ReviewThreads = true,
                        .ViewedFileState = false,
                        .ResolvePermission = false,
                        .UnresolvePermission = true,
                    },
                .AfterCursor = {},
                .ThreadId = {},
            },
    });
    expect(request.has_value(), "capability-shaped query compiles");
    expect(request->Body.find("reviewThreads") != std::string::npos
               && request->Body.find("viewerCanUnresolve") != std::string::npos
               && request->Body.find("viewerCanResolve") == std::string::npos
               && request->Body.find("viewerViewedState") == std::string::npos
               && request->Body.find("files(") == std::string::npos,
           "unsupported GraphQL fields are absent from canonical bytes");

    auto invalid = request->Semantic;
    invalid.Page = 2U;
    expect(!compile_read_request(invalid).has_value(),
           "initial GraphQL page is sequence one only");
}

auto test_successful_exhaustive_read() -> void
{
    auto transport = paged_fixture_transport{fixture_scenario::success};
    const auto result =
        perform_paged_github_read(transport,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    expect(result.has_value(), "all REST and GraphQL pages normalize");
    expect(result->Repository.Repository == core::repository_id{2001U}
               && result->PullRequest.PullRequest
                      == core::pull_request_id{3001U}
               && result->PullRequest.HeadOid
                      == application::commit_oid{std::string{head_oid}},
           "repository, pull request, and head derive from raw payloads");
    expect(result->Files.size() == 101U
               && result->Files.front().ViewedState
                      == application::github_file_viewed_state::unavailable,
           "REST Link pages exhaust and omitted viewed state is unavailable");
    expect(
        result->ReviewThreadsAvailability
                == application::github_collection_availability::available
            && result->ReviewThreads.size() == 1U
            && result->ReviewThreads.front().Comments.size() == 101U
            && result->ReviewThreads.front().ViewerResolvePermission
                   == application::github_permission_state::unavailable
            && result->ReviewThreads.front().ViewerUnresolvePermission
                   == application::github_permission_state::allowed,
        "nested comments exhaust and omitted permissions remain unavailable");
    expect(result->Commits.size() == 1U
               && result->Commits.front().Headline == "Paged read"
               && result->Reviews.size() == 1U
               && result->Reviews.front().Review
                      == application::github_review_id{"R_review"}
               && result->IssueComments.size() == 1U
               && result->IssueComments.front().Comment
                      == application::github_comment_id{"IC_comment"}
               && result->Checks.size() == 1U
               && result->Checks.front().Check
                      == application::github_check_id{"CHK_native"}
               && result->SearchResults.size() == 1U
               && result->SearchResults.front().Number
                      == core::pull_request_number{18U},
           "86-key repositories, 81-key nested PR repositories, ignored "
           "101-element arrays, and other REST supersets project only portable "
           "records");
    expect(result->RateLimits.Core.has_value()
               && result->RateLimits.Core->Remaining == 4999U
               && result->RateLimits.Search.has_value()
               && result->RateLimits.Search->Remaining == 4999U
               && result->RateLimits.Graphql.has_value()
               && result->RateLimits.Graphql->Remaining == 4999U
               && transport.Requests.size() == 11U
               && std::ranges::none_of(
                   transport.Requests,
                   [](const github_wire_request& request)
                   {
                       return request.Wire.Operation
                              == read_operation::get_viewer_identity;
                   }),
           "snapshot resource rates are observed and pull read does not repeat "
           "viewer lookup");

    auto numeric_transport =
        paged_fixture_transport{fixture_scenario::numeric_repository_link};
    const auto numeric_result =
        perform_paged_github_read(numeric_transport,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{56U},
                                  std::stop_token{});
    expect(
        numeric_result.has_value()
            && std::ranges::any_of(
                numeric_transport.Requests,
                [](const github_wire_request& request)
                {
                    return request.Wire.Operation
                               == read_operation::list_pull_request_files
                           && request.Wire.Semantic.Page == 2U
                           && request.Wire.Path
                                  == "/repos/octo-org/review-repository/"
                                     "pulls/17/files";
                }),
        "same-authority numeric repository Links are accepted while the next "
        "request remains canonical");

    auto page_first_transport =
        paged_fixture_transport{fixture_scenario::page_first_query};
    const auto page_first_result =
        perform_paged_github_read(page_first_transport,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{57U},
                                  std::stop_token{});
    expect(page_first_result.has_value(),
           "Link query parsing accepts page before per_page as well as the "
           "reversed GitHub order");

    auto enterprise_transport = paged_fixture_transport{
        fixture_scenario::success, "ghe.example.invalid"};
    const auto enterprise_result = perform_paged_github_read(
        enterprise_transport,
        enterprise_profile(),
        provider_request(
            application::github_server_contract::ghes_3_18_2022_11_28),
        core::provider_snapshot_id{56U},
        std::stop_token{});
    expect(
        enterprise_result.has_value()
            && enterprise_transport.Requests.front().Wire.Path.starts_with(
                "/api/v3/")
            && std::ranges::any_of(
                enterprise_transport.Requests,
                [](const github_wire_request& request)
                { return request.Wire.Path == "/api/graphql"; }),
        "GHES exhausts the same production pipeline under its exact namespace");
}

auto test_raw_structural_limits() -> void
{
    auto large_patch =
        paged_fixture_transport{fixture_scenario::large_ignored_patch};
    const auto large_patch_result =
        perform_paged_github_read(large_patch,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{59U},
                                  std::stop_token{});
    expect(large_patch_result.has_value()
               && large_patch_result->Files.size() == 101U,
           "ignored 500 KiB raw patches remain bounded by page bytes rather "
           "than normalized text limits");

    for(const auto scenario : {
            fixture_scenario::raw_object_ceiling_exceeded,
            fixture_scenario::raw_depth_ceiling_exceeded,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{60U},
                                      std::stop_token{});
        expect(
            !result.has_value()
                && provider_failure_of(result)
                       == application::github_provider_failure::response_limit,
            "raw object breadth and depth remain bounded");
    }

    auto page_limits = github_paged_read_limits{};
    page_limits.MaximumPageBytes = 512U;
    page_limits.MaximumAggregateResponseBytes = 1'024U;
    auto oversized_page = paged_fixture_transport{fixture_scenario::success};
    const auto page_result =
        perform_paged_github_read(oversized_page,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{61U},
                                  std::stop_token{},
                                  page_limits);
    expect(!page_result.has_value()
               && provider_failure_of(page_result)
                      == application::github_provider_failure::response_limit
               && oversized_page.Requests.size() == 1U,
           "raw body bytes remain bounded before JSON projection");

    auto aggregate_limits = github_paged_read_limits{};
    aggregate_limits.MaximumPageBytes = 128U * 1'024U;
    aggregate_limits.MaximumAggregateResponseBytes = 128U * 1'024U;
    auto aggregate =
        paged_fixture_transport{fixture_scenario::aggregate_byte_ceiling};
    const auto aggregate_result =
        perform_paged_github_read(aggregate,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{62U},
                                  std::stop_token{},
                                  aggregate_limits);
    expect(!aggregate_result.has_value()
               && provider_failure_of(aggregate_result)
                      == application::github_provider_failure::response_limit
               && aggregate.Requests.size() == 2U,
           "aggregate bytes remain bounded even when each page is admissible");

    for(const auto scenario : {
            fixture_scenario::selected_title_over_limit,
            fixture_scenario::selected_page_records_exceeded,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{68U},
                                      std::stop_token{});
        expect(
            !result.has_value()
                && provider_failure_of(result)
                       == application::github_provider_failure::response_limit,
            "selected text and page-record limits remain strict after raw "
            "structural limits are widened");
    }
}

auto test_search_form_links() -> void
{
    auto github_form =
        paged_fixture_transport{fixture_scenario::search_plus_link};
    const auto accepted =
        perform_paged_github_read(github_form,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{63U},
                                  std::stop_token{});
    expect(accepted.has_value() && accepted->SearchResults.size() == 101U,
           "Search Links accept GitHub's exact plus-for-space q form");

    for(const auto scenario : {
            fixture_scenario::search_query_semantic_drift,
            fixture_scenario::search_query_double_encoded,
            fixture_scenario::search_query_double_plus,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{64U},
                                      std::stop_token{});
        expect(!result.has_value()
                   && provider_failure_of(result)
                          == application::github_provider_failure::
                              malformed_response,
               "altered and noncanonical Search q forms fail closed");
    }
}

auto test_graphql_error_envelopes() -> void
{
    auto extensions =
        paged_fixture_transport{fixture_scenario::graphql_extensions};
    const auto extension_result =
        perform_paged_github_read(extensions,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{69U},
                                  std::stop_token{});
    expect(extension_result.has_value(),
           "GraphQL data accepts only the selected optional extensions "
           "envelope");

    for(const auto scenario : {
            fixture_scenario::graphql_errors_type_rate_limited,
            fixture_scenario::graphql_partial_errors_extension_type,
            fixture_scenario::graphql_partial_errors_extension_code,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{70U},
                                      std::stop_token{});
        const auto error = provider_error_of(result);
        expect(error.has_value()
                   && error->Failure
                          == application::github_provider_failure::rate_limit
                   && error->RetryAfterSeconds
                          == std::optional<std::uint32_t>{60U}
                   && error->RateLimitCause.has_value()
                   && error->RateLimitCause->Resource
                          == application::github_rate_limit_resource::graphql
                   && error->RateLimitCause->SecondaryLimited
                   && error->RateLimits.Core.has_value()
                   && error->RateLimits.Graphql == error->RateLimitCause,
               "GraphQL RATE_LIMITED markers fail atomically with a bounded "
               "secondary-rate fallback");
    }

    auto primary = paged_fixture_transport{
        fixture_scenario::graphql_primary_rate_metadata};
    const auto primary_result =
        perform_paged_github_read(primary,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{71U},
                                  std::stop_token{});
    const auto primary_error = provider_error_of(primary_result);
    expect(primary_error.has_value()
               && primary_error->Failure
                      == application::github_provider_failure::rate_limit
               && !primary_error->RetryAfterSeconds.has_value()
               && primary_error->RateLimitCause.has_value()
               && primary_error->RateLimitCause->Resource
                      == application::github_rate_limit_resource::graphql
               && !primary_error->RateLimitCause->SecondaryLimited,
           "GraphQL errors plus exhausted primary metadata are typed as a "
           "primary rate limit");

    for(const auto scenario : {
            fixture_scenario::graphql_malformed_errors,
            fixture_scenario::graphql_unknown_envelope_field,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{72U},
                                      std::stop_token{});
        expect(!result.has_value()
                   && provider_failure_of(result)
                          == application::github_provider_failure::
                              malformed_response,
               "malformed errors and unselected GraphQL envelope fields fail "
               "closed");
    }
}

auto test_raw_status_classification() -> void
{
    const auto verify =
        [](const fixture_scenario scenario,
           const application::github_provider_failure expected_failure,
           const std::optional<std::uint32_t> expected_retry)
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{73U},
                                      std::stop_token{});
        const auto error = provider_error_of(result);
        expect(error.has_value() && error->Failure == expected_failure
                   && error->RetryAfterSeconds == expected_retry
                   && transport.Requests.size() == 1U,
               "raw HTTP status classification preserves exact typed failure "
               "and bounded delay");
    };

    verify(fixture_scenario::rest_secondary_no_retry,
           application::github_provider_failure::rate_limit,
           60U);
    verify(fixture_scenario::rest_secondary_with_retry,
           application::github_provider_failure::rate_limit,
           17U);
    verify(fixture_scenario::rest_secondary_primary_metadata,
           application::github_provider_failure::rate_limit,
           std::nullopt);
    verify(fixture_scenario::rest_secondary_duplicate_message,
           application::github_provider_failure::permission,
           std::nullopt);
    verify(fixture_scenario::rest_generic_403,
           application::github_provider_failure::permission,
           std::nullopt);
    verify(fixture_scenario::rest_429,
           application::github_provider_failure::rate_limit,
           60U);
    verify(fixture_scenario::rest_408,
           application::github_provider_failure::timeout,
           std::nullopt);
    verify(fixture_scenario::rest_503_retry,
           application::github_provider_failure::unavailable,
           9U);
    verify(fixture_scenario::rest_504,
           application::github_provider_failure::timeout,
           std::nullopt);
}

auto test_collection_count_contract() -> void
{
    auto exact =
        paged_fixture_transport{fixture_scenario::collection_cap_exact};
    const auto accepted =
        perform_paged_github_read(exact,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{65U},
                                  std::stop_token{});
    expect(accepted.has_value() && accepted->Files.size() == 3'000U
               && accepted->Commits.size() == 250U
               && !accepted->CapabilityEvidence.ReadShape.ViewedFileState,
           "advertised file and commit counts remain exact at REST caps even "
           "when GraphQL viewed state is unavailable");

    for(const auto scenario : {
            fixture_scenario::file_count_mismatch,
            fixture_scenario::commit_count_mismatch,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{66U},
                                      std::stop_token{});
        expect(!result.has_value()
                   && provider_failure_of(result)
                          == application::github_provider_failure::
                              malformed_response,
               "terminal REST collections must equal advertised PR counts");
    }

    for(const auto scenario : {
            fixture_scenario::file_count_above_cap,
            fixture_scenario::commit_count_above_cap,
        })
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{67U},
                                      std::stop_token{});
        expect(
            !result.has_value()
                && provider_failure_of(result)
                       == application::github_provider_failure::response_limit
                && transport.Requests.size() == 2U,
            "advertised collection counts above GitHub security caps fail "
            "before capped list endpoints are trusted");
    }
}

auto test_atomic_failures() -> void
{
    const auto malformed = {
        fixture_scenario::cross_authority_link,
        fixture_scenario::wrong_numeric_repository_link,
        fixture_scenario::path_drift_link,
        fixture_scenario::fragment_link,
        fixture_scenario::userinfo_link,
        fixture_scenario::port_link,
        fixture_scenario::duplicate_link_query,
        fixture_scenario::extra_link_query,
        fixture_scenario::encoded_link_query,
        fixture_scenario::unknown_completeness,
        fixture_scenario::missing_cursor,
        fixture_scenario::repeated_cursor,
        fixture_scenario::truncated_total,
    };
    for(const auto scenario : malformed)
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{55U},
                                      std::stop_token{});
        expect(
            !result.has_value()
                && provider_failure_of(result)
                       == application::github_provider_failure::
                           malformed_response,
            "hostile pagination fails without publishing a partial snapshot");
    }

    auto once = paged_fixture_transport{fixture_scenario::network_failure};
    const auto network =
        perform_paged_github_read(once,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    expect(!network.has_value()
               && provider_failure_of(network)
                      == application::github_provider_failure::network
               && once.Requests.size() == 1U,
           "first page failure returns directly without an internal retry");

    auto wrong_resource =
        paged_fixture_transport{fixture_scenario::wrong_resource_wire_error};
    const auto rejected_resource =
        perform_paged_github_read(wrong_resource,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    expect(
        !rejected_resource.has_value()
            && provider_failure_of(rejected_resource)
                   == application::github_provider_failure::malformed_response
            && wrong_resource.Requests.size() == 1U,
        "wire errors cannot inject a rate bucket unrelated to the operation");

    auto late_wire =
        paged_fixture_transport{fixture_scenario::late_search_wire_failure};
    const auto late_failure =
        perform_paged_github_read(late_wire,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    const auto late_error = provider_error_of(late_failure);
    expect(late_error.has_value()
               && late_error->Failure
                      == application::github_provider_failure::network
               && late_error->RateLimits.Core.has_value()
               && late_error->RateLimits.Graphql.has_value()
               && late_error->RateLimits.Search.has_value()
               && late_error->RateLimits.Search->Remaining == 17U,
           "late exact wire errors preserve every accumulated resource bucket");

    const auto malformed_rest = {
        fixture_scenario::repository_missing_required,
        fixture_scenario::repository_wrong_required,
        fixture_scenario::pull_request_missing_required,
        fixture_scenario::pull_request_wrong_required,
        fixture_scenario::pull_request_missing_file_total,
        fixture_scenario::pull_request_wrong_file_total,
        fixture_scenario::pull_request_missing_commit_total,
        fixture_scenario::pull_request_wrong_commit_total,
        fixture_scenario::actor_missing_required,
        fixture_scenario::actor_wrong_required,
        fixture_scenario::file_missing_required,
        fixture_scenario::file_wrong_required,
        fixture_scenario::commit_missing_required,
        fixture_scenario::commit_wrong_required,
        fixture_scenario::review_missing_required,
        fixture_scenario::review_wrong_required,
        fixture_scenario::issue_comment_missing_required,
        fixture_scenario::issue_comment_wrong_required,
        fixture_scenario::check_missing_required,
        fixture_scenario::check_wrong_required,
        fixture_scenario::check_envelope_missing_required,
        fixture_scenario::check_envelope_wrong_required,
        fixture_scenario::search_missing_required,
        fixture_scenario::search_wrong_required,
        fixture_scenario::search_wrong_authority,
        fixture_scenario::search_wrong_owner,
        fixture_scenario::search_wrong_repository,
        fixture_scenario::search_wrong_number_url,
        fixture_scenario::search_missing_pull_request,
        fixture_scenario::search_wrong_pull_request_url,
        fixture_scenario::search_envelope_missing_required,
        fixture_scenario::search_envelope_wrong_required,
    };
    for(const auto scenario : malformed_rest)
    {
        auto transport = paged_fixture_transport{scenario};
        const auto result =
            perform_paged_github_read(transport,
                                      profile(),
                                      provider_request(),
                                      core::provider_snapshot_id{55U},
                                      std::stop_token{});
        expect(!result.has_value()
                   && provider_failure_of(result)
                          == application::github_provider_failure::
                              malformed_response,
               "each REST parser rejects missing and wrong required fields");
    }
    auto late_local =
        paged_fixture_transport{fixture_scenario::search_wrong_authority};
    const auto late_local_failure =
        perform_paged_github_read(late_local,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    const auto late_local_error = provider_error_of(late_local_failure);
    expect(
        late_local_error.has_value()
            && late_local_error->RateLimits.Core.has_value()
            && late_local_error->RateLimits.Graphql.has_value()
            && late_local_error->RateLimits.Search.has_value(),
        "late local parse failures retain accumulated resource observations");

    auto canonical_casing = paged_fixture_transport{
        fixture_scenario::search_canonical_repository_casing};
    const auto canonical_casing_result =
        perform_paged_github_read(canonical_casing,
                                  profile(),
                                  provider_request(),
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    expect(canonical_casing_result.has_value()
               && canonical_casing_result->Repository.Owner == "Octo-Org"
               && canonical_casing_result->Repository.Name
                      == "Review-Repository"
               && canonical_casing_result->SearchResults.size() == 1U,
           "search URLs bind to server-canonical repository casing");

    auto laundered_request = provider_request();
    laundered_request.CapabilityEvidence.ReadShape.ViewedFileState = true;
    auto laundered = paged_fixture_transport{fixture_scenario::success};
    const auto rejected =
        perform_paged_github_read(laundered,
                                  profile(),
                                  laundered_request,
                                  core::provider_snapshot_id{55U},
                                  std::stop_token{});
    expect(!rejected.has_value() && laundered.Requests.empty(),
           "caller cannot launder a fabricated capability-shaped read");
}
}

auto main() -> int
{
    test_metadata_and_viewer();
    test_capability_shaped_query();
    test_successful_exhaustive_read();
    test_raw_structural_limits();
    test_search_form_links();
    test_graphql_error_envelopes();
    test_raw_status_classification();
    test_collection_count_contract();
    test_atomic_failures();
    return EXIT_SUCCESS;
}
