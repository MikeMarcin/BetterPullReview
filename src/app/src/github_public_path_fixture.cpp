#include "github_public_path_fixture.hpp"

#include "github_capability_probe.hpp"
#include "github_read_contract.hpp"
#include "github_wire_transport.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::app::detail
{
namespace
{
using adapters::github::detail::capability_probe_response;
using adapters::github::detail::github_capability_probe_wire_cancelled;
using adapters::github::detail::github_capability_probe_wire_completion;
using adapters::github::detail::github_capability_probe_wire_error;
using adapters::github::detail::github_capability_probe_wire_request;
using adapters::github::detail::github_capability_probe_wire_success;
using adapters::github::detail::github_wire_cancelled;
using adapters::github::detail::github_wire_completion;
using adapters::github::detail::github_wire_error;
using adapters::github::detail::github_wire_request;
using adapters::github::detail::github_wire_success;
using adapters::github::detail::github_wire_transport_factory_port;
using adapters::github::detail::github_wire_transport_factory_result;
using adapters::github::detail::github_wire_transport_port;
using adapters::github::detail::http_header;
using adapters::github::detail::http_method;
using adapters::github::detail::page_completeness;
using adapters::github::detail::read_operation;
using adapters::github::detail::read_operation_catalog;
using adapters::github::detail::read_operation_from_name;
using adapters::github::detail::recorded_exchange;
using adapters::github::detail::server_contract;
using adapters::github::detail::strict_recorded_transport;
using adapters::github::detail::wire_response;

[[nodiscard]] auto provider_error(
    const application::github_provider_failure failure)
    -> application::github_provider_error
{
    return {
        .Failure = failure,
        .RetryAfterSeconds = std::nullopt,
        .RateLimits = {},
        .RateLimitCause = std::nullopt,
    };
}

[[nodiscard]] auto read_all(const std::filesystem::path& path) -> std::string
{
    std::ifstream input{path, std::ios::binary};
    if(!input.good())
    {
        throw std::runtime_error{"GitHub public-path fixture is unavailable"};
    }
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
    if(error.error != QJsonParseError::NoError || !document.isObject())
    {
        throw std::runtime_error{"GitHub public-path fixture is malformed"};
    }
    return document.object();
}

[[nodiscard]] auto utf8(const QString& value) -> std::string
{
    return value.toUtf8().toStdString();
}

[[nodiscard]] constexpr auto fixture_directory(
    const github_public_path_fixture fixture) noexcept -> std::string_view
{
    switch(fixture)
    {
    case github_public_path_fixture::github_com:
        return "github-com";
    case github_public_path_fixture::ghes_3_18:
        return "ghes-3.18";
    }
    return {};
}

[[nodiscard]] constexpr auto fixture_server(
    const github_public_path_fixture fixture) noexcept -> server_contract
{
    switch(fixture)
    {
    case github_public_path_fixture::github_com:
        return server_contract::github_com;
    case github_public_path_fixture::ghes_3_18:
        return server_contract::ghes_3_18;
    }
    return server_contract::github_com;
}

[[nodiscard]] auto response_headers()
    -> std::vector<http_header>
{
    return {
        {
            .Name = "Content-Type",
            .Value = "application/json",
        },
    };
}

void append_rate_limit_headers(wire_response& response,
                               const read_operation operation)
{
    auto resource = std::string{"core"};
    auto limit = std::string{"5000"};
    auto remaining = std::string{"4999"};
    if(operation == read_operation::search_pull_requests)
    {
        resource = "search";
        limit = "30";
        remaining = "29";
    }
    else if(operation == read_operation::list_review_threads)
    {
        resource = "graphql";
    }
    response.Headers.push_back(
        {.Name = "X-RateLimit-Limit", .Value = std::move(limit)});
    response.Headers.push_back(
        {.Name = "X-RateLimit-Remaining", .Value = std::move(remaining)});
    response.Headers.push_back(
        {.Name = "X-RateLimit-Reset", .Value = "2000000000"});
    response.Headers.push_back(
        {.Name = "X-RateLimit-Resource", .Value = std::move(resource)});
}

[[nodiscard]] auto load_recording(
    const github_public_path_fixture fixture)
    -> std::vector<recorded_exchange>
{
    const auto root =
        std::filesystem::path{BPR_GITHUB_CONTRACT_FIXTURE_ROOT}
        / fixture_directory(fixture);
    const auto manifest = parse_object(root / "manifest.json");
    const auto interactions = manifest.value("interactions").toArray();
    if(interactions.size()
       != static_cast<qsizetype>(read_operation_catalog.size()))
    {
        throw std::runtime_error{"GitHub public-path fixture is incomplete"};
    }

    auto exchanges = std::vector<recorded_exchange>{};
    exchanges.reserve(read_operation_catalog.size());
    for(qsizetype index = 0; index < interactions.size(); ++index)
    {
        const auto interaction = interactions[index].toObject();
        const auto operation = read_operation_from_name(
            utf8(interaction.value("operation").toString()));
        if(!operation.has_value()
           || *operation
                  != read_operation_catalog[static_cast<std::size_t>(index)])
        {
            throw std::runtime_error{
                "GitHub public-path fixture catalog is invalid"};
        }

        auto headers = std::vector<http_header>{};
        const auto safe_headers =
            interaction.value("safe_headers").toObject();
        headers.reserve(static_cast<std::size_t>(safe_headers.size()));
        for(auto item = safe_headers.constBegin();
            item != safe_headers.constEnd();
            ++item)
        {
            headers.push_back(
                {
                    .Name = utf8(item.key()),
                    .Value = utf8(item.value().toString()),
                });
        }

        const auto response_file = root
            / utf8(interaction.value("response_file").toString());
        const auto body = interaction.value("body");
        exchanges.push_back(
            {
                .Request =
                    {
                        .Semantic = {},
                        .Server = fixture_server(fixture),
                        .Operation = *operation,
                        .Method =
                            interaction.value("method").toString() == "POST"
                            ? http_method::post
                            : http_method::get,
                        .Path =
                            utf8(interaction.value("path").toString()),
                        .Query = utf8(
                            interaction.value("canonical_query").toString()),
                        .Headers = std::move(headers),
                        .Body = body.isNull()
                            ? std::string{}
                            : utf8(body.toString()),
                    },
                .Response =
                    {
                        .Status = 200,
                        .Headers = response_headers(),
                        .Body = read_all(response_file),
                        .Completeness = page_completeness::complete,
                    },
            });
    }
    return exchanges;
}

[[nodiscard]] auto load_capability_response(
    const github_public_path_fixture fixture) -> std::string
{
    return read_all(
        std::filesystem::path{BPR_GITHUB_CAPABILITY_FIXTURE_ROOT}
        / fixture_directory(fixture) / "supported.json");
}

[[nodiscard]] auto same_profile(
    const adapters::github::validated_github_host_profile& left,
    const adapters::github::validated_github_host_profile& right) noexcept
    -> bool
{
    return left.profile() == right.profile() && left.host() == right.host()
        && left.revision() == right.revision()
        && left.server_contract() == right.server_contract()
        && left.authentication_transport()
               == right.authentication_transport()
        && left.proxy_mode() == right.proxy_mode()
        && left.trust_mode() == right.trust_mode()
        && left.provenance() == right.provenance()
        && left.hostname() == right.hostname()
        && left.rest_root() == right.rest_root()
        && left.graphql_root() == right.graphql_root();
}

[[nodiscard]] auto load_bootstrap_recording(
    const github_public_path_fixture fixture)
    -> std::vector<recorded_exchange>
{
    auto exchanges = load_recording(fixture);
    exchanges.resize(1U);
    return exchanges;
}

struct fixture_recording_state
{
    fixture_recording_state(
        const github_public_path_fixture selected_fixture,
        const adapters::github::validated_github_host_profile& selected_profile)
        : Profile{selected_profile}
        , BootstrapRecording{load_bootstrap_recording(selected_fixture)}
        , Recording{load_recording(selected_fixture)}
        , CapabilityBody{load_capability_response(selected_fixture)}
    {
    }

    adapters::github::validated_github_host_profile Profile;
    strict_recorded_transport BootstrapRecording;
    strict_recorded_transport Recording;
    std::string CapabilityBody;
    std::size_t CapabilityCalls = 0U;
    std::size_t BootstrapFactoryCalls = 0U;
    std::size_t AccountFactoryCalls = 0U;
    std::size_t AccountReadCalls = 0U;
    int LastReadOperation = -1;
    int RecordingFailure = -1;
    mutable std::mutex Mutex;
};

enum class fixture_transport_role
{
    bootstrap,
    account,
};

class fixture_recorded_wire_transport final
    : public github_wire_transport_port
{
public:
    explicit fixture_recorded_wire_transport(
        std::shared_ptr<fixture_recording_state> state,
        const fixture_transport_role role)
        : State{std::move(state)}
        , Role{role}
    {
    }

    [[nodiscard]] auto perform(
        const github_wire_request& request,
        const std::stop_token stop) -> github_wire_completion override
    {
        if(stop.stop_requested())
        {
            return github_wire_cancelled{.Correlation = request.Correlation};
        }
        const std::scoped_lock lock{State->Mutex};
        if(request.Correlation.ProfileRevision != State->Profile.revision()
           || request.Correlation.Host != State->Profile.host())
        {
            State->RecordingFailure = -2;
            return github_wire_error{
                .Correlation = request.Correlation,
                .Error = provider_error(
                    application::github_provider_failure::malformed_response),
            };
        }
        auto& recording =
            Role == fixture_transport_role::bootstrap
            ? State->BootstrapRecording
            : State->Recording;
        auto response = recording.perform(request.Wire);
        if(!response.has_value())
        {
            State->RecordingFailure =
                static_cast<int>(response.error().Code);
            return github_wire_error{
                .Correlation = request.Correlation,
                .Error = provider_error(
                    application::github_provider_failure::malformed_response),
            };
        }
        if(Role == fixture_transport_role::account)
        {
            ++State->AccountReadCalls;
            State->LastReadOperation =
                static_cast<int>(request.Wire.Operation);
        }
        append_rate_limit_headers(*response, request.Wire.Operation);
        return github_wire_success{
            .Correlation = request.Correlation,
            .Response = std::move(*response),
        };
    }

    [[nodiscard]] auto perform_capability_probe(
        const github_capability_probe_wire_request& request,
        const std::stop_token stop)
        -> github_capability_probe_wire_completion override
    {
        if(stop.stop_requested())
        {
            return github_capability_probe_wire_cancelled{
                .Correlation = request.Correlation,
            };
        }
        const std::scoped_lock lock{State->Mutex};
        const auto expected =
            adapters::github::detail::compile_capability_probe_request(
                State->Profile);
        if(!expected.has_value() || request.Wire != *expected
           || request.Correlation.ProfileRevision
                  != State->Profile.revision()
           || request.Correlation.Host != State->Profile.host()
           || request.Correlation.Session.Profile
                  != State->Profile.profile()
           || request.Correlation.ObservedAccount.value() == 0U
           || State->CapabilityCalls != 0U)
        {
            return github_capability_probe_wire_error{
                .Correlation = request.Correlation,
                .Error = provider_error(
                    application::github_provider_failure::malformed_response),
            };
        }
        ++State->CapabilityCalls;
        auto response = capability_probe_response{
            .Status = 200,
            .Authenticated = true,
            .ContentType = "application/json",
            .Body = State->CapabilityBody,
            .RateLimitHeaders = {},
            .RateLimitHeaderCount = 4U,
        };
        response.RateLimitHeaders[0U] = {
            .Name = "X-RateLimit-Limit",
            .Value = "5000",
        };
        response.RateLimitHeaders[1U] = {
            .Name = "X-RateLimit-Remaining",
            .Value = "4999",
        };
        response.RateLimitHeaders[2U] = {
            .Name = "X-RateLimit-Reset",
            .Value = "2000000000",
        };
        response.RateLimitHeaders[3U] = {
            .Name = "X-RateLimit-Resource",
            .Value = "graphql",
        };
        return github_capability_probe_wire_success{
            .Correlation = request.Correlation,
            .Response = std::move(response),
        };
    }

private:
    std::shared_ptr<fixture_recording_state> State;
    fixture_transport_role Role;
};

class fixture_transport_factory final
    : public github_wire_transport_factory_port
{
public:
    explicit fixture_transport_factory(
        std::shared_ptr<fixture_recording_state> state)
        : State{std::move(state)}
        , BootstrapTransport{
              std::make_shared<fixture_recorded_wire_transport>(
                  State, fixture_transport_role::bootstrap)}
        , AccountTransport{
              std::make_shared<fixture_recorded_wire_transport>(
                  State, fixture_transport_role::account)}
    {
    }

    [[nodiscard]] auto make_bootstrap(
        const adapters::github::validated_github_host_profile& profile,
        const core::github_session_request_context context,
        application::github_credential_material credential) noexcept
        -> github_wire_transport_factory_result override
    {
        const std::scoped_lock lock{State->Mutex};
        if(!same_profile(profile, State->Profile)
           || context.Profile != profile.profile()
           || !application::valid(credential)
           || State->BootstrapFactoryCalls != 0U)
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::unavailable)};
        }
        ++State->BootstrapFactoryCalls;
        return BootstrapTransport;
    }

    [[nodiscard]] auto make_account(
        const adapters::github::validated_github_host_profile& profile,
        application::github_credential_retrieve_success credential) noexcept
        -> github_wire_transport_factory_result override
    {
        const std::scoped_lock lock{State->Mutex};
        if(!same_profile(profile, State->Profile)
           || !application::valid(credential.Credential)
           || credential.Context.Authority.Profile != profile.profile()
           || credential.Context.Authority.ProfileRevision
                  != profile.revision()
           || credential.Context.Authority.Host != profile.host()
           || credential.Context.Authority.Hostname.value()
                  != profile.hostname()
           || credential.Context.Authority.ServerContract
                  != profile.server_contract()
           || State->AccountFactoryCalls != 0U)
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::unavailable)};
        }
        ++State->AccountFactoryCalls;
        return AccountTransport;
    }

private:
    std::shared_ptr<fixture_recording_state> State;
    std::shared_ptr<fixture_recorded_wire_transport> BootstrapTransport;
    std::shared_ptr<fixture_recorded_wire_transport> AccountTransport;
};

struct fixture_vault_state
{
    std::optional<application::github_credential_operation_context> Context;
    std::optional<application::github_credential_material> Credential;
    bool Stored = false;
    bool Retrieved = false;
    bool Deleted = false;
    mutable std::mutex Mutex;
};

class fixture_credential_vault final
    : public application::github_credential_vault_port
{
public:
    fixture_credential_vault(
        application::effect_executor& effects,
        std::shared_ptr<fixture_vault_state> state)
        : Effects{effects}
        , State{std::move(state)}
    {
    }

    void store(
        application::github_credential_enrollment enrollment,
        application::github_credential_store_completion_handler completion)
        override
    {
        auto pending = std::make_shared<
            application::github_credential_enrollment>(
            std::move(enrollment));
        Effects.post(
            [state = State,
             pending = std::move(pending),
             completion = std::move(completion)]() mutable
            {
                if(!application::valid(*pending))
                {
                    completion(application::github_credential_store_error{
                        .Context = pending->Context,
                        .Failure = application::
                            github_credential_vault_failure::invalid_request,
                    });
                    return;
                }
                const auto context = pending->Context;
                {
                    const std::scoped_lock lock{state->Mutex};
                    state->Context = context;
                    state->Credential =
                        std::move(pending->Credential);
                    state->Stored = true;
                }
                completion(application::github_credential_store_success{
                    .Context = context,
                    .Disposition = application::
                        github_credential_store_disposition::stored,
                });
            });
    }

    void retrieve(
        const application::github_credential_operation_context context,
        application::github_credential_retrieve_completion_handler completion)
        override
    {
        Effects.post(
            [state = State, context, completion = std::move(completion)]() mutable
            {
                auto material = std::optional<
                    application::github_credential_material>{};
                {
                    const std::scoped_lock lock{state->Mutex};
                    if(state->Context == context
                       && state->Credential.has_value())
                    {
                        auto copy =
                            application::github_credential_material::create(
                                state->Credential->bytes());
                        if(copy.has_value())
                        {
                            material.emplace(std::move(*copy));
                            state->Retrieved = true;
                        }
                    }
                }
                if(!material.has_value())
                {
                    completion(application::github_credential_retrieve_error{
                        .Context = context,
                        .Failure = application::
                            github_credential_vault_failure::operation_failed,
                    });
                    return;
                }
                completion(application::github_credential_retrieve_success{
                    .Context = context,
                    .Credential = std::move(*material),
                });
            });
    }

    void delete_credential(
        const application::github_credential_operation_context context,
        application::github_credential_delete_completion_handler completion)
        override
    {
        Effects.post(
            [state = State, context, completion = std::move(completion)]() mutable
            {
                auto disposition =
                    application::github_credential_delete_disposition::absent;
                auto exact_context = true;
                {
                    const std::scoped_lock lock{state->Mutex};
                    if(state->Context.has_value()
                       && state->Context != context)
                    {
                        exact_context = false;
                    }
                    else
                    {
                        if(state->Credential.has_value())
                        {
                            state->Credential.reset();
                            disposition = application::
                                github_credential_delete_disposition::deleted;
                        }
                        state->Context.reset();
                        state->Deleted = true;
                    }
                }
                if(!exact_context)
                {
                    completion(application::github_credential_delete_error{
                        .Context = context,
                        .Failure = application::
                            github_credential_vault_failure::invalid_request,
                    });
                    return;
                }
                completion(application::github_credential_delete_success{
                    .Context = context,
                    .Disposition = disposition,
                });
            });
    }

    void cancel(
        const application::github_credential_operation_context) override
    {
        // Cancellation is advisory. Every accepted fixture task still emits
        // its one terminal so shutdown can establish exact cleanup.
    }

private:
    application::effect_executor& Effects;
    std::shared_ptr<fixture_vault_state> State;
};

[[nodiscard]] auto make_fixture_credential()
    -> application::github_credential_material
{
    auto bytes = std::array<std::byte, 32>{};
    std::random_device random;
    for(auto& byte : bytes)
    {
        byte = static_cast<std::byte>(random() & 0xffU);
    }
    auto material = application::github_credential_material::create(
        std::span<const std::byte>{bytes});
    std::ranges::fill(bytes, std::byte{});
    if(!material.has_value())
    {
        throw std::runtime_error{
            "GitHub public-path fixture credential allocation failed"};
    }
    return std::move(*material);
}
}

class github_public_path_fixture_components::implementation final
{
public:
    implementation(
        const github_public_path_fixture fixture,
        const adapters::github::validated_github_host_profile& profile,
        application::effect_executor& vault_effects)
        : Recording{
              std::make_shared<fixture_recording_state>(fixture, profile)}
        , Factory{std::make_shared<fixture_transport_factory>(Recording)}
        , VaultState{std::make_shared<fixture_vault_state>()}
        , Vault{
              std::make_unique<fixture_credential_vault>(
                  vault_effects, VaultState)}
    {
    }

    std::shared_ptr<fixture_recording_state> Recording;
    std::shared_ptr<fixture_transport_factory> Factory;
    std::shared_ptr<fixture_vault_state> VaultState;
    std::unique_ptr<fixture_credential_vault> Vault;
};

github_public_path_fixture_components::
    github_public_path_fixture_components(
        const github_public_path_fixture fixture,
        const adapters::github::validated_github_host_profile& profile,
        application::effect_executor& vault_effects)
    : Implementation{std::make_unique<implementation>(
          fixture, profile, vault_effects)}
{
}

github_public_path_fixture_components::
    ~github_public_path_fixture_components() = default;

auto github_public_path_fixture_components::transport_factory() const noexcept
    -> std::shared_ptr<github_wire_transport_factory_port>
{
    return Implementation->Factory;
}

auto github_public_path_fixture_components::credential_vault() const noexcept
    -> application::github_credential_vault_port&
{
    return *Implementation->Vault;
}

auto github_public_path_fixture_components::take_enrollment_credential()
    -> application::github_credential_material
{
    return make_fixture_credential();
}

auto github_public_path_fixture_components::evidence() const noexcept
    -> github_public_path_fixture_evidence
{
    auto result = github_public_path_fixture_evidence{};
    {
        const std::scoped_lock lock{Implementation->Recording->Mutex};
        result.RecordingConsumed =
            Implementation->Recording->BootstrapRecording.consumed_all()
            && Implementation->Recording->Recording.consumed_all();
        result.CapabilityProbeConsumed =
            Implementation->Recording->CapabilityCalls == 1U;
        result.BootstrapTransportSelected =
            Implementation->Recording->BootstrapFactoryCalls == 1U;
        result.AccountTransportSelected =
            Implementation->Recording->AccountFactoryCalls == 1U;
        result.RecordingFailure =
            Implementation->Recording->RecordingFailure;
        result.AccountReadCount = static_cast<int>(
            Implementation->Recording->AccountReadCalls);
        result.LastReadOperation =
            Implementation->Recording->LastReadOperation;
    }
    {
        const std::scoped_lock lock{Implementation->VaultState->Mutex};
        result.CredentialStored = Implementation->VaultState->Stored;
        result.CredentialRetrieved = Implementation->VaultState->Retrieved;
        result.CredentialDeleted = Implementation->VaultState->Deleted;
        result.SecretRetained =
            Implementation->VaultState->Credential.has_value();
    }
    return result;
}
}
