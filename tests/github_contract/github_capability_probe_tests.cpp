#include "github_capability_probe.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::adapters::github::detail;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] auto fixture_root() -> std::filesystem::path
{
#if defined(BPR_GITHUB_CAPABILITY_FIXTURE_ROOT)
    return BPR_GITHUB_CAPABILITY_FIXTURE_ROOT;
#elif defined(BPR_GITHUB_CONTRACT_FIXTURE_ROOT)
    return std::filesystem::path{BPR_GITHUB_CONTRACT_FIXTURE_ROOT}
               .parent_path()
               .parent_path()
        / "capabilities/v1";
#else
    return std::filesystem::path{__FILE__}.parent_path().parent_path()
        / "fixtures/github/capabilities/v1";
#endif
}

[[nodiscard]] auto read_file(const std::filesystem::path& path)
    -> std::string
{
    std::ifstream input{path, std::ios::binary};
    expect(input.good(), "capability fixture opens");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

[[nodiscard]] auto parse(const std::string& text) -> QJsonDocument
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        QByteArray{
            text.data(),
            static_cast<qsizetype>(text.size()),
        },
        &error);
    expect(
        error.error == QJsonParseError::NoError
            && document.isObject(),
        "test fixture JSON parses");
    return document;
}

[[nodiscard]] auto compact(const QJsonDocument& document) -> std::string
{
    const auto bytes = document.toJson(QJsonDocument::Compact);
    return {
        bytes.constData(),
        static_cast<std::size_t>(bytes.size()),
    };
}

[[nodiscard]] auto sha256(const std::string_view value) -> QString
{
    return QCryptographicHash::hash(
               QByteArray{
                   value.data(),
                   static_cast<qsizetype>(value.size()),
               },
               QCryptographicHash::Sha256)
        .toHex();
}

[[nodiscard]] auto reverse_arrays(const QJsonValue& value) -> QJsonValue
{
    if(value.isArray())
    {
        const auto source = value.toArray();
        auto reversed = QJsonArray{};
        for(auto index = source.size(); index > 0; --index)
        {
            reversed.push_back(reverse_arrays(source.at(index - 1)));
        }
        return reversed;
    }
    if(value.isObject())
    {
        auto reversed = QJsonObject{};
        const auto source = value.toObject();
        for(auto iterator = source.begin(); iterator != source.end(); ++iterator)
        {
            reversed.insert(iterator.key(), reverse_arrays(iterator.value()));
        }
        return reversed;
    }
    return value;
}

[[nodiscard]] auto load_supported_fixture(
    const std::filesystem::path& path) -> std::string
{
    return read_file(path);
}

[[nodiscard]] auto load_manifest_fixture(
    const std::filesystem::path& directory,
    const QString& expected_product,
    const QString& expected_version,
    const qsizetype expected_query_fields,
    const qsizetype expected_mutation_fields,
    const qsizetype expected_repository_fields,
    const qsizetype expected_actor_possible_types,
    const qsizetype expected_node_possible_types,
    const qsizetype expected_pull_request_fields) -> std::string
{
    const auto manifest =
        parse(read_file(directory / "manifest.json")).object();
    const auto response_file = manifest.value("response_file").toString();
    const auto projection = manifest.value("projection").toObject();
    const auto expected = manifest.value("expected").toObject();
    expect(manifest.value("schema_revision").toInt() == 1
               && manifest.value("product").toString() == expected_product
               && manifest.value("version").toString() == expected_version
               && response_file == QStringLiteral("supported.json"),
           "capability fixture manifest identifies the exact host contract");
    for(const auto key : {
            "review_threads",
            "viewed_file_mutations",
            "resolve_thread",
            "unresolve_thread",
        })
    {
        expect(expected.value(key).toString() == QStringLiteral("supported"),
               "fixture manifest records each expected capability");
    }

    const auto body = read_file(directory / response_file.toStdString());
    const auto response = parse(body);
    const auto data = response.object().value("data").toObject();
    const auto actual_query_fields = data.value("schemaRoot")
                                         .toObject()
                                         .value("queryType")
                                         .toObject()
                                         .value("fields")
                                         .toArray()
                                         .size();
    const auto actual_mutation_fields = data.value("schemaRoot")
                                            .toObject()
                                            .value("mutationType")
                                            .toObject()
                                            .value("fields")
                                            .toArray()
                                            .size();
    const auto actual_repository_fields =
        data.value("repository").toObject().value("fields").toArray().size();
    const auto actual_actor_possible_types = data.value("actorPossibleTypes")
                                                 .toObject()
                                                 .value("possibleTypes")
                                                 .toArray()
                                                 .size();
    const auto actual_node_possible_types = data.value("nodePossibleTypes")
                                                .toObject()
                                                .value("possibleTypes")
                                                .toArray()
                                                .size();
    const auto actual_pull_request_fields =
        data.value("pullRequest").toObject().value("fields").toArray().size();
    expect(actual_query_fields == expected_query_fields
               && actual_mutation_fields == expected_mutation_fields
               && actual_repository_fields == expected_repository_fields
               && actual_actor_possible_types == expected_actor_possible_types
               && actual_node_possible_types == expected_node_possible_types
               && actual_pull_request_fields == expected_pull_request_fields
               && projection.value("query_fields").toInt()
                      == expected_query_fields
               && projection.value("mutation_fields").toInt()
                      == expected_mutation_fields
               && projection.value("repository_fields").toInt()
                      == expected_repository_fields
               && projection.value("actor_possible_types").toInt()
                      == expected_actor_possible_types
               && projection.value("node_possible_types").toInt()
                      == expected_node_possible_types
               && projection.value("pull_request_fields").toInt()
                      == expected_pull_request_fields,
           "manifest field counts match its independent response");
    expect(projection.value("query_sha256").toString()
                   == sha256(capability_probe_graphql_document())
               && projection.value("response_sha256").toString()
                      == sha256(body),
           "manifest hashes bind the exact query and response bytes");
    expect(static_cast<std::size_t>(actual_query_fields)
                   <= capability_probe_limits{}.MaximumQueryFields
               && static_cast<std::size_t>(actual_mutation_fields)
                      <= capability_probe_limits{}.MaximumMutationFields
               && static_cast<std::size_t>(actual_repository_fields)
                      <= capability_probe_limits{}.MaximumRepositoryFields
               && static_cast<std::size_t>(actual_actor_possible_types)
                      <= capability_probe_limits{}.MaximumNamedTypes
               && static_cast<std::size_t>(actual_node_possible_types)
                      <= capability_probe_limits{}.MaximumNamedTypes
               && static_cast<std::size_t>(
                   actual_pull_request_fields)
                <= capability_probe_limits{}.MaximumPullRequestFields,
        "hard defaults admit the official complete schema sizes");
    return body;
}

[[nodiscard]] auto set_mutation_fields(
    QJsonDocument document,
    QJsonArray fields) -> QJsonDocument
{
    auto envelope = document.object();
    auto data = envelope.value("data").toObject();
    auto schema = data.value("schemaRoot").toObject();
    auto mutation = schema.value("mutationType").toObject();
    mutation.insert("fields", std::move(fields));
    schema.insert("mutationType", mutation);
    data.insert("schemaRoot", schema);
    envelope.insert("data", data);
    return QJsonDocument{envelope};
}

[[nodiscard]] auto mutation_fields(const QJsonDocument& document)
    -> QJsonArray
{
    return document.object()
        .value("data")
        .toObject()
        .value("schemaRoot")
        .toObject()
        .value("mutationType")
        .toObject()
        .value("fields")
        .toArray();
}

[[nodiscard]] auto synthetic_fixture(
    const std::filesystem::path& path) -> std::string
{
    const auto descriptor = parse(read_file(path)).object();
    const auto base = path.parent_path()
        / descriptor.value("base_response_file").toString().toStdString();
    auto document = parse(read_file(base));
    auto fields = mutation_fields(document);
    const auto target = descriptor.value("field").toString();
    const auto operation = descriptor.value("operation").toString();

    QJsonArray changed;
    for(const auto& value : fields)
    {
        auto field = value.toObject();
        if(
            field.value("name").toString() == target
            && operation == QStringLiteral("remove_field"))
        {
            continue;
        }
        if(
            field.value("name").toString() == target
            && operation == QStringLiteral("replace_return_type"))
        {
            field.insert("type", descriptor.value("replacement"));
        }
        changed.push_back(field);
    }
    return compact(set_mutation_fields(document, std::move(changed)));
}

[[nodiscard]] auto github_com_candidate() -> github_host_profile_candidate
{
    return {
        .Profile = core::host_profile_id{11},
        .Host = core::git_host_id{21},
        .Revision = application::github_profile_revision{31},
        .Server =
            application::github_server_contract::github_com_2022_11_28,
        .Authentication =
            application::github_authentication_transport::github_cli,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = github_profile_provenance::builtin,
        .Hostname = "github.com",
        .RestRoot = "https://api.github.com",
        .GraphqlRoot = "https://api.github.com/graphql",
    };
}

[[nodiscard]] auto enterprise_candidate() -> github_host_profile_candidate
{
    return {
        .Profile = core::host_profile_id{12},
        .Host = core::git_host_id{22},
        .Revision = application::github_profile_revision{32},
        .Server =
            application::github_server_contract::ghes_3_18_2022_11_28,
        .Authentication =
            application::github_authentication_transport::native_https,
        .Proxy = application::github_proxy_mode::system,
        .Trust = application::github_trust_mode::system,
        .Provenance = github_profile_provenance::explicit_user,
        .Hostname = "ghe.example.invalid",
        .RestRoot = "https://ghe.example.invalid/api/v3",
        .GraphqlRoot = "https://ghe.example.invalid/api/graphql",
    };
}

[[nodiscard]] auto observe(
    std::string body,
    const bool authenticated = true,
    const int status = 200,
    std::string content_type = "application/json",
    const capability_probe_limits& limits = {},
    const std::initializer_list<capability_probe_header>
        rate_limit_headers = {})
    -> capability_probe_observation
{
    expect(
        rate_limit_headers.size()
            <= maximum_capability_probe_rate_limit_headers,
        "test rate-limit facts fit the bounded response");
    auto bounded_headers = std::array<
        capability_probe_header,
        maximum_capability_probe_rate_limit_headers>{};
    auto header_index = std::size_t{0U};
    for(const auto& header : rate_limit_headers)
    {
        bounded_headers[header_index++] = header;
    }

    const capability_probe_transport_result response{
        capability_probe_response{
            .Status = status,
            .Authenticated = authenticated,
            .ContentType = std::move(content_type),
            .Body = std::move(body),
            .RateLimitHeaders = std::move(bounded_headers),
            .RateLimitHeaderCount = rate_limit_headers.size(),
        },
    };
    return normalize_capability_probe(response, limits);
}

[[nodiscard]] auto fact(
    const capability_probe_observation& observation,
    const capability_schema_fact value)
    -> capability_schema_fact_state
{
    return observation.Facts[static_cast<std::size_t>(value)];
}

auto expect_all_unknown(
    const capability_probe_observation& observation,
    const capability_probe_unavailability reason,
    const std::string_view message) -> void
{
    expect(!observation.Complete, message);
    expect(observation.Unavailability == reason, message);
    expect(
        observation.Capabilities == core::github_capabilities{},
        message);
    for(const auto state : observation.Facts)
    {
        expect(
            state == capability_schema_fact_state::unknown,
            message);
    }
    expect(valid(observation), "unavailable observation invariant holds");
}

auto expect_all_supported(
    const capability_probe_observation& observation,
    const std::string_view message) -> void
{
    expect(observation.Complete, message);
    expect(
        observation.Unavailability
            == capability_probe_unavailability::none,
        message);
    expect(
        observation.Capabilities
            == core::github_capabilities{
                .ReviewThreads =
                    core::github_capability_state::supported,
                .ViewedFileMutations =
                    core::github_capability_state::supported,
                .ResolveThread =
                    core::github_capability_state::supported,
                .UnresolveThread =
                    core::github_capability_state::supported,
            },
        message);
    for(const auto state : observation.Facts)
    {
        expect(
            state == capability_schema_fact_state::matched,
            message);
    }
    expect(valid(observation), "complete observation invariant holds");
}

auto test_requests_bind_validated_profile_authority() -> void
{
    const auto github_profile =
        validate_github_host_profile(github_com_candidate());
    const auto enterprise_profile =
        validate_github_host_profile(enterprise_candidate());
    expect(
        github_profile.has_value() && enterprise_profile.has_value(),
        "probe profiles validate");

    const auto copied_github_profile = *github_profile;
    const auto github_request =
        compile_capability_probe_request(copied_github_profile);
    expect(github_request.has_value(), "GitHub.com probe compiles");
    expect(
        github_request->Profile == github_profile->profile()
            && github_request->Host == github_profile->host()
            && github_request->ProfileRevision
                == github_profile->revision()
            && github_request->Contract
                == github_profile->server_contract()
            && github_request->Method
                == capability_probe_http_method::post
            && valid(github_request->Method),
        "probe carries exact validated profile identity");
    expect(
        github_request->Authority == "api.github.com"
            && github_request->Root
                == "https://api.github.com/graphql"
            && github_request->Path == "/graphql",
        "GitHub.com probe binds exact validated GraphQL authority");

    const auto enterprise_request =
        compile_capability_probe_request(*enterprise_profile);
    expect(enterprise_request.has_value(), "GHES probe compiles");
    expect(
        enterprise_request->Authority == "ghe.example.invalid"
            && enterprise_request->Root
                == "https://ghe.example.invalid/api/graphql"
            && enterprise_request->Path == "/api/graphql"
            && enterprise_request->Contract
                == enterprise_profile->server_contract()
            && enterprise_request->Body == github_request->Body,
        "GHES probe binds exact validated GraphQL authority");
    expect(
        github_request->Headers
            == std::array<capability_probe_header, 3>{
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
        "probe uses the exact non-secret GraphQL headers");

    const auto payload = parse(github_request->Body).object();
    expect(
        payload.size() == 1 && payload.value("query").isString(),
        "probe body contains only the static query");
    const auto query = payload.value("query").toString().toStdString();
    expect(
        query == capability_probe_graphql_document()
            && query.starts_with(
                "query BetterPullReviewCapabilityProbeV1")
            && capability_probe_document_is_query_only(query)
            && sha256(query)
                == QStringLiteral("892ad33dcb73ef8b9ed7a4e4e6f6e628"
                                        "568e4de5267eca87ed246122701feb10"),
           "probe operation is a query and never a mutation document");
    expect(
        capability_probe_document_is_query_only(
            R"(query Safe{mutationType{name} probe(value:"mutation"){name}} fragment MutationFields on __Type{name})")
            && !capability_probe_document_is_query_only(
                "mutation "
                "Unsafe{markFileAsViewed(input:{}){clientMutationId}}")
            && !capability_probe_document_is_query_only(
                "query Safe{__typename} mutation Unsafe{__typename}")
            && !capability_probe_document_is_query_only(
                "query Broken{__typename"),
        "operation parser distinguishes mutation names from operations");
    expect(!valid(static_cast<capability_probe_http_method>(255U)),
           "unknown HTTP methods fail closed");

    auto limits = capability_probe_limits{};
    limits.MaximumRequestBytes = 1U;
    const auto bounded =
        compile_capability_probe_request(*github_profile, limits);
    expect(!bounded.has_value()
               && bounded.error()
                      == capability_probe_unavailability::response_limit,
           "request byte limit fails closed");

    limits = {};
    limits.MaximumDepth += 1U;
    const auto invalid =
        compile_capability_probe_request(*github_profile, limits);
    expect(!invalid.has_value()
               && invalid.error()
                      == capability_probe_unavailability::invalid_limits,
           "probe limits cannot relax hard ceilings");
}

auto test_paired_supported_contracts() -> void
{
    const auto root = fixture_root();
    const auto github_body = load_manifest_fixture(
        root / "github-com", QStringLiteral("github.com"),
        QStringLiteral("cloud"), 31, 271, 145, 5, 280, 113);
    const auto enterprise_body = load_manifest_fixture(
        root / "ghes-3.18", QStringLiteral("github-enterprise-server"),
        QStringLiteral("3.18"), 24, 224, 133, 5, 244,
        109);
    const auto github = observe(github_body);
    const auto enterprise = observe(enterprise_body);
    expect_all_supported(github, "GitHub.com schema supports all facts");
    expect_all_supported(
        enterprise,
        "GHES 3.18 schema supports all facts");
    expect(
        github == enterprise,
        "paired host contracts normalize to identical evidence");

    const auto reordered = observe(compact(QJsonDocument{
        reverse_arrays(parse(github_body).object()).toObject()}));
    expect_all_supported(
        reordered,
        "schema member names are independent of response order");
    expect(
        reordered == github,
        "schema reordering preserves exact normalized evidence");
}

auto test_synthetic_missing_and_incompatible_contracts() -> void
{
    const auto root = fixture_root();
    const auto missing = observe(synthetic_fixture(
        root / "synthetic/missing-unresolve.json"));
    expect(missing.Complete, "missing field is complete schema evidence");
    expect(
        missing.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && missing.Capabilities.ViewedFileMutations
                == core::github_capability_state::supported
            && missing.Capabilities.ResolveThread
                == core::github_capability_state::supported
            && missing.Capabilities.UnresolveThread
                == core::github_capability_state::unsupported,
        "missing unresolve affects only its dependent capability");
    expect(
        fact(
            missing,
            capability_schema_fact::unresolve_review_thread)
            == capability_schema_fact_state::missing,
        "missing mutation is retained as exact evidence");
    expect(valid(missing), "mixed complete observation is valid");

    const auto incompatible = observe(synthetic_fixture(
        root / "synthetic/incompatible-mark.json"));
    expect(incompatible.Complete, "wrong signature is complete evidence");
    expect(
        incompatible.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && incompatible.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && incompatible.Capabilities.ResolveThread
                == core::github_capability_state::supported
            && incompatible.Capabilities.UnresolveThread
                == core::github_capability_state::supported,
        "incompatible mark signature disables combined viewed mutations");
    expect(
        fact(
            incompatible,
            capability_schema_fact::mark_file_as_viewed)
            == capability_schema_fact_state::incompatible,
        "incompatible signature is retained as exact evidence");
    expect(valid(incompatible), "incompatible complete evidence is valid");
}

[[nodiscard]] auto with_data_alias_removed(
    const std::string& body,
    const QString& alias) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    data.remove(alias);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_data_alias_null(
    const std::string& body,
    const QString& alias) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    data.insert(alias, QJsonValue::Null);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_alias_member(
    const std::string& body,
    const QString& alias,
    const QString& member,
    const QJsonValue& replacement) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    type.insert(member, replacement);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto without_named_field(
    const std::string& body,
    const QString& alias,
    const QString& field_name) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    const auto fields = type.value("fields").toArray();
    auto retained = QJsonArray{};
    for(const auto& value : fields)
    {
        if(
            value.toObject().value("name").toString()
            != field_name)
        {
            retained.push_back(value);
        }
    }
    expect(
        retained.size() + 1 == fields.size(),
        "targeted schema field exists exactly once");
    type.insert("fields", retained);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_named_field_type(const std::string& body,
                                         const QString& alias,
                                         const QString& field_name,
                                         QJsonObject replacement) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    auto fields = type.value("fields").toArray();
    auto replacements = 0;
    for(auto index = 0; index < fields.size(); ++index)
    {
        auto field = fields.at(index).toObject();
        if(field.value("name").toString() == field_name)
        {
            field.insert("type", std::move(replacement));
            fields.replace(index, field);
            ++replacements;
        }
    }
    expect(replacements == 1, "targeted schema field exists exactly once");
    type.insert("fields", fields);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto without_schema_root_field(const std::string& body,
                                             const QString& root_name,
                                             const QString& field_name)
    -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto schema = data.value("schemaRoot").toObject();
    auto root = schema.value(root_name).toObject();
    const auto fields = root.value("fields").toArray();
    auto retained = QJsonArray{};
    for(const auto& value : fields)
    {
        if(value.toObject().value("name").toString() != field_name)
        {
            retained.push_back(value);
        }
    }
    expect(retained.size() + 1 == fields.size(),
           "targeted schema-root field exists exactly once");
    root.insert("fields", retained);
    schema.insert(root_name, root);
    data.insert("schemaRoot", schema);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_schema_root_field_type(const std::string& body,
                                               const QString& root_name,
                                               const QString& field_name,
                                               const QJsonObject& replacement)
    -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto schema = data.value("schemaRoot").toObject();
    auto root = schema.value(root_name).toObject();
    auto fields = root.value("fields").toArray();
    auto replacements = 0;
    for(auto index = 0; index < fields.size(); ++index)
    {
        auto field = fields.at(index).toObject();
        if(field.value("name").toString() == field_name)
        {
            field.insert("type", replacement);
            fields.replace(index, field);
            ++replacements;
        }
    }
    expect(replacements == 1, "targeted schema-root field exists exactly once");
    root.insert("fields", fields);
    schema.insert(root_name, root);
    data.insert("schemaRoot", schema);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto
without_named_type_member(const std::string& body, const QString& alias,
                          const QString& member_name, const QString& type_name)
    -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    const auto members = type.value(member_name).toArray();
    auto retained = QJsonArray{};
    for(const auto& value : members)
    {
        if(value.toObject().value("name").toString() != type_name)
        {
            retained.push_back(value);
        }
    }
    expect(retained.size() + 1 == members.size(),
           "targeted named type exists exactly once");
    type.insert(member_name, retained);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_graphql_errors(const std::string& body) -> std::string
{
    auto envelope = parse(body).object();
    envelope.insert("errors",
                    QJsonArray{
                        QJsonObject{{"message", "sanitized probe error"}},
                    });
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_rate_limited_graphql_errors(
    const std::string& body) -> std::string
{
    auto envelope = parse(body).object();
    envelope.insert(
        "errors",
        QJsonArray{
            QJsonObject{
                {"message", "sanitized throttling error"},
                {"type", "RATE_LIMITED"},
            },
        });
    return compact(QJsonDocument{envelope});
}

auto test_partial_and_unavailable_evidence_stays_unknown() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");

    expect_all_unknown(
        observe(with_graphql_errors(supported)),
        capability_probe_unavailability::partial_response,
        "GraphQL errors reject all partial evidence");
    expect_all_unknown(
        observe(with_data_alias_removed(supported, "resolvePayload")),
        capability_probe_unavailability::partial_response,
        "missing alias rejects all partial evidence");
    expect_all_unknown(
        observe(supported, false),
        capability_probe_unavailability::authentication,
        "unauthenticated schema cannot prove unsupported");
    expect_all_unknown(
        observe(supported, true, 401),
        capability_probe_unavailability::authentication,
        "HTTP authentication failure remains unknown");
    expect_all_unknown(
        observe(supported, true, 403),
        capability_probe_unavailability::permission,
        "HTTP permission failure remains unknown");
    expect_all_unknown(
        observe(supported, true, 404),
        capability_probe_unavailability::api_version,
        "missing endpoint remains unknown");
    expect_all_unknown(
        observe(supported, true, 429),
        capability_probe_unavailability::rate_limit,
        "rate limit remains unknown");
    expect_all_unknown(
        observe(supported, true, 500),
        capability_probe_unavailability::invalid_status,
        "other HTTP failure remains unknown");
    expect_all_unknown(
        observe(supported, true, 200, "text/html"),
        capability_probe_unavailability::invalid_content_type,
        "non-JSON response remains unknown");

    for(const auto failure : {
            capability_probe_unavailability::network,
            capability_probe_unavailability::certificate,
            capability_probe_unavailability::redirect,
            capability_probe_unavailability::timeout,
            capability_probe_unavailability::cancelled,
        })
    {
        const capability_probe_transport_result transport{
            std::unexpected{failure}};
        expect_all_unknown(
            normalize_capability_probe(transport),
            failure,
            "transport failure cannot become unsupported");
    }

    const capability_probe_transport_result malformed_reason{
        std::unexpected{
            static_cast<capability_probe_unavailability>(255U)}};
    expect_all_unknown(
        normalize_capability_probe(malformed_reason),
        capability_probe_unavailability::malformed_response,
        "malformed transport reason fails closed to a valid category");
}

auto test_rate_limit_classification_is_bounded() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");
    const auto errors = with_graphql_errors(supported);

    expect_all_unknown(
        observe(
            errors,
            true,
            200,
            "application/json",
            {},
            {
                {"x-ratelimit-limit", "5000"},
                    {"x-ratelimit-remaining", "0"},
                    {"x-ratelimit-reset", "2000000000"},
                    {"x-ratelimit-resource", "graphql"},
                }),
        capability_probe_unavailability::rate_limit,
        "GitHub GraphQL 200 exhaustion is classified as rate limit");
    expect_all_unknown(observe(supported, true, 403, "application/json", {},
                               {{"Retry-After", "60"}}),
                       capability_probe_unavailability::rate_limit,
                       "GitHub 403 secondary limit uses bounded Retry-After");
    expect_all_unknown(
        observe(supported, true, 403, "application/json", {},
                {
                    {"X-RateLimit-Limit", "5000"},
                    {"X-RateLimit-Remaining", " \t0\t "},
                    {"X-RateLimit-Reset", "2000000000"},
                    {"X-RateLimit-Resource", "graphql"},
                }),
        capability_probe_unavailability::rate_limit,
        "rate-limit numeric headers admit only outer HTTP whitespace");
    expect_all_unknown(observe(with_rate_limited_graphql_errors(supported)),
                       capability_probe_unavailability::rate_limit,
                       "structured GraphQL RATE_LIMITED error is classified");
    expect_all_unknown(
        observe(with_rate_limited_graphql_errors(supported), true, 403),
        capability_probe_unavailability::rate_limit,
        "GraphQL 403 structured RATE_LIMITED error is classified");
    expect_all_unknown(
        observe(errors, true, 200, "application/json", {},
                {
                    {"X-RateLimit-Limit", "5000"},
                    {"X-RateLimit-Remaining", "1"},
                    {"X-RateLimit-Reset", "2000000000"},
                    {"X-RateLimit-Resource", "graphql"},
                }),
        capability_probe_unavailability::partial_response,
        "non-exhausted GraphQL errors remain generic partial evidence");
    expect_all_supported(observe(supported, true, 200, "application/json", {},
                                 {
                                     {"X-RateLimit-Limit", "5000"},
                                     {"X-RateLimit-Remaining", "0"},
                                     {"X-RateLimit-Reset", "2000000000"},
                                     {"X-RateLimit-Resource", "graphql"},
                                 }),
                         "a complete successful observation remains usable");

    const auto expect_malformed_headers =
        [&supported](
            const std::initializer_list<capability_probe_header> headers,
            const std::string_view message) {
            expect_all_unknown(
                observe(supported, true, 403, "application/json", {}, headers),
                capability_probe_unavailability::malformed_response, message);
        };
    expect_malformed_headers({{"X-Unbounded-Provider-Header", "0"}},
                             "non-allowlisted response headers are rejected");
    expect_malformed_headers(
        {
            {"X-RateLimit-Remaining", "0"},
            {"x-ratelimit-remaining", "0"},
        },
        "case-insensitive duplicate rate-limit headers are rejected");
    expect_malformed_headers({{"X-RateLimit-Remaining", "-1"}},
                             "signed rate-limit values are rejected");
    expect_malformed_headers({{"X-RateLimit-Remaining", "0\r\nInjected: 1"}},
                             "header-value injection is rejected");
    expect_malformed_headers(
        {{"X-RateLimit-Remaining", "18446744073709551616"}},
        "overflowing rate-limit values are rejected");
    expect_malformed_headers(
        {
            {"X-RateLimit-Limit", "5"},
            {"X-RateLimit-Remaining", "6"},
            {"X-RateLimit-Reset", "2000000000"},
            {"X-RateLimit-Resource", "graphql"},
        },
        "inconsistent rate-limit facts are rejected");
    expect_malformed_headers(
        {
            {"X-RateLimit-Limit", "5"},
            {"X-RateLimit-Remaining", "4"},
            {"X-RateLimit-Reset", "2000000000"},
            {"X-RateLimit-Resource", "core"},
        },
        "capability response cannot inject a non-GraphQL rate resource");
    expect_malformed_headers(
        {{"Retry-After", std::string(33U, '9')}},
        "oversized rate-limit values are rejected");

    auto malformed_response = capability_probe_response{
        .Status = 403,
        .Authenticated = true,
        .ContentType = "application/json",
        .Body = supported,
        .RateLimitHeaders = {},
        .RateLimitHeaderCount =
            maximum_capability_probe_rate_limit_headers + 1U,
    };
    expect_all_unknown(
        normalize_capability_probe(
            capability_probe_transport_result{malformed_response}),
        capability_probe_unavailability::malformed_response,
        "rate-limit header count cannot exceed fixed storage");

    malformed_response.RateLimitHeaderCount = 0U;
    malformed_response.RateLimitHeaders.front() = {
        .Name = "X-RateLimit-Remaining",
        .Value = "0",
    };
    expect_all_unknown(
        normalize_capability_probe(
            capability_probe_transport_result{malformed_response}),
        capability_probe_unavailability::malformed_response,
        "unaccounted trailing response facts are rejected");
}

auto test_complete_absence_is_unsupported() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");
    const auto missing_object =
        observe(with_data_alias_null(supported, "reviewThread"));
    expect(
        missing_object.Complete
            && missing_object.Capabilities.ReviewThreads
                == core::github_capability_state::unsupported
            && fact(
                   missing_object,
                   capability_schema_fact::review_threads_read)
                == capability_schema_fact_state::missing
            && valid(missing_object),
        "null object alias is definitive missing schema evidence");

    const auto missing_input =
        observe(with_data_alias_null(supported, "markInput"));
    expect(
        missing_input.Complete
            && missing_input.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && fact(
                   missing_input,
                   capability_schema_fact::mark_file_as_viewed)
                == capability_schema_fact_state::missing
            && valid(missing_input),
        "null input alias is definitive missing schema evidence");

    const auto missing_enum =
        observe(with_data_alias_null(supported, "fileViewedState"));
    expect(missing_enum.Complete, "null __type is complete absence");
    expect(
        missing_enum.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && missing_enum.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && missing_enum.Capabilities.ResolveThread
                == core::github_capability_state::supported
            && missing_enum.Capabilities.UnresolveThread
                == core::github_capability_state::supported,
        "definitively absent viewed enum is unsupported");
    expect(
        fact(
            missing_enum,
            capability_schema_fact::viewed_file_state_read)
            == capability_schema_fact_state::missing,
        "absent type remains missing evidence");
    expect(valid(missing_enum), "complete missing-type evidence is valid");
}

auto test_null_selected_members_are_incomplete() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");
    expect_all_unknown(
        observe(with_alias_member(
            supported,
            QStringLiteral("reviewThread"),
            QStringLiteral("fields"),
            QJsonValue::Null)),
        capability_probe_unavailability::malformed_response,
        "null fields on an object cannot prove schema absence");
    expect_all_unknown(
        observe(with_alias_member(
            supported,
            QStringLiteral("markInput"),
            QStringLiteral("inputFields"),
            QJsonValue::Null)),
        capability_probe_unavailability::malformed_response,
        "null inputFields on an input object cannot prove absence");
    expect_all_unknown(
        observe(with_alias_member(
            supported,
            QStringLiteral("fileViewedState"),
            QStringLiteral("enumValues"),
            QJsonValue::Null)),
        capability_probe_unavailability::malformed_response,
        "null enumValues on an enum cannot prove absence");
}

auto test_capability_dependencies_remain_independent() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");
    const auto missing_resolve_permission = observe(without_named_field(
        supported,
        QStringLiteral("reviewThread"),
        QStringLiteral("viewerCanResolve")));
    expect(
        missing_resolve_permission.Complete
            && missing_resolve_permission.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && missing_resolve_permission.Capabilities.ResolveThread
                == core::github_capability_state::unsupported
            && missing_resolve_permission.Capabilities.UnresolveThread
                == core::github_capability_state::supported
            && fact(
                   missing_resolve_permission,
                   capability_schema_fact::resolve_permission)
                == capability_schema_fact_state::missing
            && valid(missing_resolve_permission),
        "resolve permission is independent from thread readability");

    const auto missing_unresolve_permission = observe(without_named_field(
        supported,
        QStringLiteral("reviewThread"),
        QStringLiteral("viewerCanUnresolve")));
    expect(
        missing_unresolve_permission.Complete
            && missing_unresolve_permission.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && missing_unresolve_permission.Capabilities.ResolveThread
                == core::github_capability_state::supported
            && missing_unresolve_permission.Capabilities.UnresolveThread
                == core::github_capability_state::unsupported
            && fact(
                   missing_unresolve_permission,
                   capability_schema_fact::unresolve_permission)
                == capability_schema_fact_state::missing
            && valid(missing_unresolve_permission),
        "unresolve permission is independent from thread readability");

    const auto missing_viewed_state = observe(without_named_field(
        supported,
        QStringLiteral("changedFile"),
        QStringLiteral("viewerViewedState")));
    expect(
        missing_viewed_state.Complete
            && missing_viewed_state.Capabilities.ReviewThreads
                == core::github_capability_state::supported
            && missing_viewed_state.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && missing_viewed_state.Capabilities.ResolveThread
                == core::github_capability_state::supported
            && missing_viewed_state.Capabilities.UnresolveThread
                == core::github_capability_state::supported
            && fact(
                   missing_viewed_state,
                   capability_schema_fact::viewed_file_state_read)
                == capability_schema_fact_state::missing
            && valid(missing_viewed_state),
        "viewed-state reads are independent from review threads");
}

auto test_compiled_connection_counts_are_required() -> void
{
    const auto supported =
        load_supported_fixture(fixture_root() / "github-com/supported.json");
    const auto wrong_type = QJsonObject{
        {"kind", "NON_NULL"},
        {"name", QJsonValue::Null},
        {
            "ofType",
            QJsonObject{
                {"kind", "SCALAR"},
                {"name", "String"},
            },
        },
    };

    for(const auto& alias : {
            QStringLiteral("threadConnection"),
            QStringLiteral("commentConnection"),
        })
    {
        const auto missing = observe(without_named_field(
            supported, alias, QStringLiteral("totalCount")));
        expect(missing.Complete
                   && missing.Capabilities.ReviewThreads
                          == core::github_capability_state::unsupported
                   && missing.Capabilities.ViewedFileMutations
                          == core::github_capability_state::supported
                   && missing.Capabilities.ResolveThread
                          == core::github_capability_state::unsupported
                   && missing.Capabilities.UnresolveThread
                          == core::github_capability_state::unsupported
                   && fact(missing, capability_schema_fact::review_threads_read)
                          == capability_schema_fact_state::missing
                   && valid(missing),
               "missing review connection totalCount disables its read shape");

        const auto incompatible = observe(with_named_field_type(
            supported, alias, QStringLiteral("totalCount"), wrong_type));
        expect(
            incompatible.Complete
                && fact(incompatible,
                        capability_schema_fact::review_threads_read)
                       == capability_schema_fact_state::incompatible
                && incompatible.Capabilities.ReviewThreads
                       == core::github_capability_state::unsupported
                && valid(incompatible),
            "incompatible review connection totalCount disables its read shape");
    }

    const auto missing_files = observe(
        without_named_field(supported, QStringLiteral("changedFileConnection"),
                            QStringLiteral("totalCount")));
    expect(missing_files.Complete
               && missing_files.Capabilities.ReviewThreads
                      == core::github_capability_state::supported
               && missing_files.Capabilities.ViewedFileMutations
                      == core::github_capability_state::unsupported
               && fact(missing_files,
                       capability_schema_fact::viewed_file_state_read)
                      == capability_schema_fact_state::missing
               && valid(missing_files),
           "missing file connection totalCount disables viewed-state reads");

    const auto incompatible_files = observe(with_named_field_type(
        supported, QStringLiteral("changedFileConnection"),
        QStringLiteral("totalCount"), wrong_type));
    expect(
        incompatible_files.Complete
            && incompatible_files.Capabilities.ReviewThreads
                   == core::github_capability_state::supported
            && incompatible_files.Capabilities.ViewedFileMutations
                   == core::github_capability_state::unsupported
            && fact(incompatible_files,
                    capability_schema_fact::viewed_file_state_read)
                   == capability_schema_fact_state::incompatible
            && valid(incompatible_files),
        "incompatible file connection totalCount disables viewed-state reads");
}

auto test_compiled_graphql_baseline_is_required() -> void
{
    const auto supported =
        load_supported_fixture(fixture_root() / "github-com/supported.json");
    const auto wrong_type = QJsonObject{
        {"kind", "NON_NULL"},
        {"name", QJsonValue::Null},
        {
            "ofType",
            QJsonObject{
                {"kind", "SCALAR"},
                {"name", "String"},
            },
        },
    };
    const auto expect_facts =
        [](const capability_probe_observation& observation,
           const capability_schema_fact_state review,
           const capability_schema_fact_state viewed,
           const std::string_view message) {
            expect(observation.Complete
                       && fact(observation,
                               capability_schema_fact::review_threads_read)
                              == review
                       && fact(observation,
                               capability_schema_fact::viewed_file_state_read)
                              == viewed
                       && valid(observation),
                   message);
        };

    expect_facts(observe(without_schema_root_field(
                     supported, QStringLiteral("queryType"),
                     QStringLiteral("repository"))),
                 capability_schema_fact_state::missing,
                 capability_schema_fact_state::missing,
                 "missing Query.repository disables both compiled read shapes");
    expect_facts(
        observe(with_schema_root_field_type(
            supported, QStringLiteral("queryType"),
            QStringLiteral("repository"), wrong_type)),
        capability_schema_fact_state::incompatible,
        capability_schema_fact_state::incompatible,
        "incompatible Query.repository disables both compiled read shapes");

    for(const auto& field_name : {
            QStringLiteral("id"),
            QStringLiteral("pullRequest"),
        })
    {
        expect_facts(
            observe(without_named_field(supported, QStringLiteral("repository"),
                                        field_name)),
            capability_schema_fact_state::missing,
            capability_schema_fact_state::missing,
            "missing Repository baseline field disables both read shapes");
        expect_facts(
            observe(with_named_field_type(supported,
                                          QStringLiteral("repository"),
                                          field_name, wrong_type)),
            capability_schema_fact_state::incompatible,
            capability_schema_fact_state::incompatible,
            "incompatible Repository baseline field disables both read shapes");
    }

    expect_facts(
        observe(without_named_field(supported, QStringLiteral("pullRequest"),
                                    QStringLiteral("id"))),
        capability_schema_fact_state::missing,
        capability_schema_fact_state::missing,
        "missing PullRequest.id disables both compiled read shapes");
    expect_facts(
        observe(with_named_field_type(supported, QStringLiteral("pullRequest"),
                                      QStringLiteral("id"), wrong_type)),
        capability_schema_fact_state::incompatible,
        capability_schema_fact_state::incompatible,
        "incompatible PullRequest.id disables both compiled read shapes");

    expect_facts(observe(without_schema_root_field(supported,
                                                   QStringLiteral("queryType"),
                                                   QStringLiteral("node"))),
                 capability_schema_fact_state::missing,
                 capability_schema_fact_state::matched,
                 "missing Query.node disables only comment paging");
    expect_facts(observe(with_schema_root_field_type(
                     supported, QStringLiteral("queryType"),
                     QStringLiteral("node"), wrong_type)),
                 capability_schema_fact_state::incompatible,
                 capability_schema_fact_state::matched,
                 "incompatible Query.node disables only comment paging");

    expect_facts(
        observe(with_data_alias_null(supported,
                                     QStringLiteral("reviewThreadInterfaces"))),
        capability_schema_fact_state::missing,
        capability_schema_fact_state::matched,
        "missing review-thread interface evidence disables comment paging");
    expect_facts(
        observe(without_named_type_member(
            supported, QStringLiteral("reviewThreadInterfaces"),
            QStringLiteral("interfaces"), QStringLiteral("Node"))),
        capability_schema_fact_state::incompatible,
        capability_schema_fact_state::matched,
        "review thread without Node compatibility disables comment paging");

    expect_facts(
        observe(with_data_alias_null(supported,
                                     QStringLiteral("actorPossibleTypes"))),
        capability_schema_fact_state::missing,
        capability_schema_fact_state::matched,
        "missing Actor possible-type evidence disables author identity reads");
    const auto actor_type = parse(supported)
                                .object()
                                .value("data")
                                .toObject()
                                .value("actorPossibleTypes")
                                .toObject()
                                .value("possibleTypes")
                                .toArray()
                                .at(0)
                                .toObject()
                                .value("name")
                                .toString();
    expect(!actor_type.isEmpty(), "fixture has an Actor possible type");
    expect_facts(
        observe(without_named_type_member(
            supported, QStringLiteral("nodePossibleTypes"),
            QStringLiteral("possibleTypes"), actor_type)),
        capability_schema_fact_state::incompatible,
        capability_schema_fact_state::matched,
        "one non-Node Actor disables total author identity normalization");
}

[[nodiscard]] auto with_duplicate_review_field(
    const std::string& body) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto thread = data.value("reviewThread").toObject();
    auto fields = thread.value("fields").toArray();
    expect(!fields.isEmpty(), "review thread fixture has fields");
    fields.push_back(fields.at(0));
    thread.insert("fields", fields);
    data.insert("reviewThread", thread);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_duplicate_alias_member(
    const std::string& body,
    const QString& alias,
    const QString& member_name) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    auto members = type.value(member_name).toArray();
    expect(!members.isEmpty(), "schema fixture member list is nonempty");
    members.push_back(members.at(0));
    type.insert(member_name, members);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_duplicate_mark_argument(
    const std::string& body) -> std::string
{
    auto document = parse(body);
    auto fields = mutation_fields(document);
    auto changed = QJsonArray{};
    for(const auto& value : fields)
    {
        auto field = value.toObject();
        if(
            field.value("name").toString()
            == QStringLiteral("markFileAsViewed"))
        {
            auto arguments = field.value("args").toArray();
            expect(!arguments.isEmpty(), "mark mutation fixture has arguments");
            arguments.push_back(arguments.at(0));
            field.insert("args", arguments);
        }
        changed.push_back(field);
    }
    return compact(set_mutation_fields(document, std::move(changed)));
}

[[nodiscard]] auto with_extra_mark_argument(
    const std::string& body,
    const bool required) -> std::string
{
    auto document = parse(body);
    auto fields = mutation_fields(document);
    QJsonArray changed;
    for(const auto& value : fields)
    {
        auto field = value.toObject();
        if(
            field.value("name").toString()
            == QStringLiteral("markFileAsViewed"))
        {
            auto arguments = field.value("args").toArray();
            arguments.push_back(QJsonObject{
                {"name", "futureArgument"},
                {"defaultValue", QJsonValue::Null},
                {
                    "type",
                    required
                        ? QJsonValue{QJsonObject{
                              {"kind", "NON_NULL"},
                              {"name", QJsonValue::Null},
                              {
                                  "ofType",
                                  QJsonObject{
                                      {"kind", "SCALAR"},
                                      {"name", "String"},
                                  },
                              },
                          }}
                        : QJsonValue{QJsonObject{
                              {"kind", "SCALAR"},
                              {"name", "String"},
                              {"ofType", QJsonValue::Null},
                          }},
                },
            });
            field.insert("args", arguments);
        }
        changed.push_back(field);
    }
    return compact(set_mutation_fields(document, std::move(changed)));
}

[[nodiscard]] auto with_extra_mark_input_field(
    const std::string& body,
    const bool required) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto input = data.value("markInput").toObject();
    auto fields = input.value("inputFields").toArray();
    fields.push_back(QJsonObject{
        {"name", "futureInputField"},
        {"defaultValue", QJsonValue::Null},
        {
            "type",
            required
                ? QJsonValue{QJsonObject{
                      {"kind", "NON_NULL"},
                      {"name", QJsonValue::Null},
                      {
                          "ofType",
                          QJsonObject{
                              {"kind", "SCALAR"},
                              {"name", "String"},
                          },
                      },
                  }}
                : QJsonValue{QJsonObject{
                      {"kind", "SCALAR"},
                      {"name", "String"},
                      {"ofType", QJsonValue::Null},
                  }},
        },
    });
    input.insert("inputFields", fields);
    data.insert("markInput", input);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto without_enum_value(
    const std::string& body,
    const QString& alias,
    const QString& name) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value(alias).toObject();
    const auto values = type.value("enumValues").toArray();
    auto retained = QJsonArray{};
    for(const auto& value : values)
    {
        if(value.toObject().value("name").toString() != name)
        {
            retained.push_back(value);
        }
    }
    expect(
        retained.size() + 1 == values.size(),
        "targeted enum value exists exactly once");
    type.insert("enumValues", retained);
    data.insert(alias, type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

[[nodiscard]] auto with_wrong_enum_member_shape(
    const std::string& body) -> std::string
{
    auto envelope = parse(body).object();
    auto data = envelope.value("data").toObject();
    auto type = data.value("fileViewedState").toObject();
    auto values = type.value("enumValues").toArray();
    expect(!values.isEmpty(), "viewed-state enum fixture has values");
    values.replace(0, QJsonObject{{"name", 7}});
    type.insert("enumValues", values);
    data.insert("fileViewedState", type);
    envelope.insert("data", data);
    return compact(QJsonDocument{envelope});
}

auto test_malformed_duplicates_additions_and_limits() -> void
{
    const auto supported = load_supported_fixture(
        fixture_root() / "github-com/supported.json");

    expect_all_unknown(
        observe(R"({"data":{},"data":{}})"),
        capability_probe_unavailability::malformed_response,
        "duplicate JSON key rejects all evidence");
    expect_all_unknown(
        observe("{"),
        capability_probe_unavailability::malformed_response,
        "malformed JSON rejects all evidence");
    expect_all_unknown(
        observe(with_duplicate_review_field(supported)),
        capability_probe_unavailability::malformed_response,
        "duplicate introspection field name rejects all evidence");
    expect_all_unknown(
        observe(with_duplicate_mark_argument(supported)),
        capability_probe_unavailability::malformed_response,
        "duplicate introspection argument name rejects all evidence");
    expect_all_unknown(
        observe(with_duplicate_alias_member(
            supported,
            QStringLiteral("markInput"),
            QStringLiteral("inputFields"))),
        capability_probe_unavailability::malformed_response,
        "duplicate introspection input name rejects all evidence");
    expect_all_unknown(
        observe(with_duplicate_alias_member(
            supported,
            QStringLiteral("fileViewedState"),
            QStringLiteral("enumValues"))),
        capability_probe_unavailability::malformed_response,
        "duplicate introspection enum name rejects all evidence");
    expect_all_unknown(
        observe(with_wrong_enum_member_shape(supported)),
        capability_probe_unavailability::malformed_response,
        "wrong enum member shape rejects all evidence");

    const auto missing_enum_value = observe(without_enum_value(
        supported,
        QStringLiteral("fileViewedState"),
        QStringLiteral("VIEWED")));
    expect(
        missing_enum_value.Complete
            && missing_enum_value.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && fact(
                   missing_enum_value,
                   capability_schema_fact::viewed_file_state_read)
                == capability_schema_fact_state::incompatible
            && valid(missing_enum_value),
        "missing required enum value is complete incompatibility");

    const auto additive_optional =
        observe(with_extra_mark_argument(supported, false));
    expect_all_supported(
        additive_optional,
        "additive optional argument preserves compatibility");

    const auto additive_required =
        observe(with_extra_mark_argument(supported, true));
    expect(additive_required.Complete, "required addition is complete");
    expect(
        additive_required.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && fact(
                   additive_required,
                   capability_schema_fact::mark_file_as_viewed)
                == capability_schema_fact_state::incompatible,
        "additive required argument is incompatible");
    expect(valid(additive_required), "required-addition evidence is valid");

    const auto optional_input =
        observe(with_extra_mark_input_field(supported, false));
    expect_all_supported(
        optional_input,
        "additive optional input field preserves compatibility");

    const auto required_input =
        observe(with_extra_mark_input_field(supported, true));
    expect(
        required_input.Complete
            && required_input.Capabilities.ViewedFileMutations
                == core::github_capability_state::unsupported
            && fact(
                   required_input,
                   capability_schema_fact::mark_file_as_viewed)
                == capability_schema_fact_state::incompatible
            && valid(required_input),
        "additive required input field is incompatible");

    const auto expect_response_limit =
        [&supported](
            const capability_probe_limits& limits,
            const std::string_view message) {
            expect_all_unknown(
                observe(
                    supported,
                    true,
                    200,
                    "application/json; charset=utf-8",
                    limits),
                capability_probe_unavailability::response_limit,
                message);
        };

    auto limits = capability_probe_limits{};
    limits.MaximumQueryFields = 30U;
    expect_response_limit(limits, "query field limit rejects all evidence");

    limits = {};
    limits.MaximumMutationFields = 270U;
    expect_response_limit(limits, "mutation field limit rejects all evidence");

    limits = {};
    limits.MaximumRepositoryFields = 144U;
    expect_response_limit(limits,
                          "repository field limit rejects all evidence");

    limits = {};
    limits.MaximumPullRequestFields = 112U;
    expect_response_limit(limits,
                          "pull-request field limit rejects all evidence");

    limits = {};
    limits.MaximumOtherTypeFields = 46U;
    expect_response_limit(
        limits, "other selected-type field limit rejects all evidence");

    limits = {};
    limits.MaximumNamedTypes = 279U;
    expect_response_limit(limits,
                          "named-type membership limit rejects all evidence");

    limits = {};
    limits.MaximumArrayElements = 3U;
    expect_response_limit(
        limits,
        "global array limit rejects all evidence");

    limits = {};
    limits.MaximumDepth = 2U;
    expect_response_limit(
        limits,
        "JSON depth limit rejects all evidence");

    limits = {};
    limits.MaximumObjectMembers = 2U;
    expect_response_limit(
        limits,
        "JSON object-member limit rejects all evidence");

    limits = {};
    limits.MaximumArguments = 1U;
    expect_response_limit(
        limits,
        "schema argument limit rejects all evidence");

    limits = {};
    limits.MaximumInputFields = 2U;
    expect_response_limit(
        limits,
        "schema input-field limit rejects all evidence");

    limits = {};
    limits.MaximumEnumValues = 2U;
    expect_response_limit(
        limits,
        "schema enum-value limit rejects all evidence");

    limits = {};
    limits.MaximumTotalValues = 100U;
    expect_response_limit(
        limits,
        "JSON total-value limit rejects all evidence");

    limits = {};
    limits.MaximumStringBytes = 4U;
    expect_response_limit(
        limits,
        "JSON string-byte limit rejects all evidence");

    std::string oversized(
        capability_probe_limits{}.MaximumResponseBytes + 1U,
        'x');
    expect_all_unknown(
        observe(std::move(oversized)),
        capability_probe_unavailability::response_limit,
        "response byte limit rejects all evidence");
}

auto test_observation_invariants_reject_tampering() -> void
{
    const auto supported = observe(load_supported_fixture(
        fixture_root() / "github-com/supported.json"));
    expect(valid(supported), "baseline complete observation is valid");

    auto tampered = supported;
    tampered.Unavailability = capability_probe_unavailability::network;
    expect(
        !valid(tampered),
        "complete evidence cannot carry an unavailable reason");

    tampered = supported;
    tampered.Facts.front() = capability_schema_fact_state::unknown;
    expect(
        !valid(tampered),
        "complete evidence cannot contain an unknown fact");

    tampered = supported;
    tampered.Capabilities.ResolveThread =
        core::github_capability_state::unsupported;
    expect(
        !valid(tampered),
        "capabilities must agree with their exact dependency facts");

    auto invalid_incomplete = capability_probe_observation{};
    invalid_incomplete.Unavailability =
        capability_probe_unavailability::none;
    expect(
        !valid(invalid_incomplete),
        "incomplete evidence requires a valid unavailable reason");

    invalid_incomplete = {};
    invalid_incomplete.Capabilities.ReviewThreads =
        core::github_capability_state::supported;
    expect(
        !valid(invalid_incomplete),
        "incomplete evidence cannot expose a supported capability");

    invalid_incomplete = {};
    invalid_incomplete.Facts.front() =
        capability_schema_fact_state::matched;
    expect(
        !valid(invalid_incomplete),
        "incomplete evidence cannot retain partial schema facts");
}
} // namespace

int main()
{
    test_requests_bind_validated_profile_authority();
    test_paired_supported_contracts();
    test_synthetic_missing_and_incompatible_contracts();
    test_partial_and_unavailable_evidence_stays_unknown();
    test_rate_limit_classification_is_bounded();
    test_complete_absence_is_unsupported();
    test_null_selected_members_are_incomplete();
    test_capability_dependencies_remain_independent();
    test_compiled_connection_counts_are_required();
    test_compiled_graphql_baseline_is_required();
    test_malformed_duplicates_additions_and_limits();
    test_observation_invariants_reject_tampering();
    std::cout << "github capability probe contracts passed\n";
    return EXIT_SUCCESS;
}
