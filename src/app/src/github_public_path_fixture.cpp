#include "github_public_path_fixture.hpp"

#include "github_capability_probe.hpp"
#include "github_read_contract.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
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
using adapters::github::detail::page_completeness;
using adapters::github::detail::parse_recorded_http_method;
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

class deterministic_git_fixture final
{
public:
    // Fixed packs from the documented synthetic three-commit graph keep
    // process ownership in the installed-Git adapter while still exercising
    // strict connected-pack import through the public application.
    deterministic_git_fixture()
        : PackOne{QByteArray::fromBase64(QByteArrayLiteral(
              "UEFDSwAAAAIAAAAGmxF4nK3MQYoDIRQE0L2ncD8Q/BpbhWEYssh6yA3UXxKh0x3ETnL89HTIDVKrWryq3gCpYsqRvHFsGKS8L8ki5AFFcxiY2Or9ANLiGhumLo12VjPlDMtw1kWyDl6Z4lViDq7oFIMOJOLSz3OTB/SO9reM4wm3irs81kdfGuR3eZXftInrKtomdnW6xbHyjySn9mTNoJT8UmtEni+X+q8/eyve4zMiy3mCeAJlmFx++QXcHI17MYyzpgBcUSfDps7iPwWv1Hicm830nFG3pCg1VcEs2cw4zSgxxdwwzcDAPM3EIDHVzCLRKNXAPNHU3NLUwMDU0sTEMsloYqwHi4GZudHElcHSINrAQEHbAAi4uNwyK0pKi1IVkhKLU7kAbA4YjaYCeJwzNDAwMzFRKEoty0wt1yupKGH4fPBX26vslbVSuyOFua10V+ycb/YVABWTD/23AXicS0osTuVKyywqLlEoSi3LTC1XyEhNTOECAGX8CDymAnicMzQwMDMxUShKLctMLdcrqShhuD+tetrSyidREjtUxY0uZrBsepoVCgAI0Q8jNXicS0osTuUCAAWgAaZd2MNtPA/zt//f/8VU8oXGklEwcw=="))}
        , PackTwo{QByteArray::fromBase64(QByteArrayLiteral(
              "UEFDSwAAAAIAAAAJmxF4nK2OzQrCMBCE73mK3AXZNE3Tgoh48Cy+wWazoYH+EdLWx7et+AZehmH4ZpicmCU5ALCEqsHGeW0pNM40RQCtjCfFyiGbStViwsRDlltGtbdOq5qcxgrAkFGFpc0Tc6EDGAy+FDjndkzyzjlzes5d9+Il8iof8Z3nxPISvubmDmLaiHQQ5zgs2EV/lcpCWZTbP5CnXQWNfR93+r+z4lduGb3M6yg+qs9c5ZsReJytzEGKAyEUBNC9p3A/EPwaW4VhGLLIesgN1F8SodMdxE5y/PR0yA1Sq1q8qt4AqWLKkbxxbBikvC/JIuQBRXMYmNjq/QDS4hobpi6NdlYz5QzLcNZFsg5emeJVYg6u6BSDDiTi0s9zkwf0jva3jOMJt4q7PNZHXxrkd3mV37SJ6yraJnZ1usWx8o8kp/ZkzaCU/FJrRJ4vl/qvP3sr3uMzIst5gngCZZhcfvkF3ByNezGMs6YAXFEnw6bO4j8Fr9R4nJvN9JxRt6QoNVXBLNnMOM0oMcXcMM3AwDzNxCAx1cwi0SjVwDzR1NzS1MDA1NLExDLJaGKsB4uBmbnRxJXB0iDawEBB2wAIuLjcMitKSotSFZISi1O5AGwOGI2mAnicMzQwMDMxUShKLctMLdcrqShhcFjne/qkEb/Ql0Xxc11cujZIizxxBQD6eg4+uAF4nEtKLE7lKk5Nzs9LUShKLctMLVfISE1M4QIAblgIkKYCeJwzNDAwMzFRKEoty0wt1yupKGH4fPBX26vslbVSuyOFua10V+ycb/YVABWTD/23AXicS0osTuVKyywqLlEoSi3LTC1XyEhNTOECAGX8CDymAnicMzQwMDMxUShKLctMLdcrqShhuD+tetrSyidREjtUxY0uZrBsepoVCgAI0Q8jNXicS0osTuUCAAWgAaZYF/iPSkRZM7LhDWRJt/Byd2lT5g=="))}
    {
        if(PackOne.size() < 32 || PackTwo.size() < 32
           || !PackOne.startsWith("PACK") || !PackTwo.startsWith("PACK"))
        {
            throw std::runtime_error{"Git fixture packs are invalid"};
        }
    }

    std::string Base = "32752d1cce5de757a157e803f80bdd97f2ba9291";
    std::string HeadOne = "dc1c8d7b318cb3a6005c5127c3a6cee23f05afd4";
    std::string HeadTwo = "f16d0557f942dd5359088238ada3cf8295cfabfc";
    QByteArray PackOne;
    QByteArray PackTwo;
};

void replace_all(std::string& value,
                 const std::string_view from,
                 const std::string_view to)
{
    auto position = std::size_t{0U};
    while((position = value.find(from, position)) != std::string::npos)
    {
        value.replace(position, from.size(), to);
        position += to.size();
    }
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
    const github_public_path_fixture fixture,
    const deterministic_git_fixture& git,
    const std::string_view head)
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
        const auto method = parse_recorded_http_method(
            utf8(interaction.value("method").toString()));
        if(!method.has_value())
        {
            throw std::runtime_error{
                "GitHub public-path fixture method is invalid"};
        }
        auto request_path = utf8(interaction.value("path").toString());
        auto request_query =
            utf8(interaction.value("canonical_query").toString());
        auto request_body = body.isNull()
            ? std::string{}
            : utf8(body.toString());
        replace_all(request_path,
                    "2222222222222222222222222222222222222222",
                    head);
        replace_all(request_query,
                    "2222222222222222222222222222222222222222",
                    head);
        replace_all(request_body,
                    "2222222222222222222222222222222222222222",
                    head);
        auto response_body = read_all(response_file);
        replace_all(response_body,
                    "1111111111111111111111111111111111111111",
                    git.Base);
        const auto observed_head =
            *operation == read_operation::list_reviews
            ? std::string_view{git.HeadOne}
            : head;
        replace_all(response_body,
                    "2222222222222222222222222222222222222222",
                    observed_head);
        exchanges.push_back(
            {
                .Request =
                    {
                        .Semantic = {},
                        .Server = fixture_server(fixture),
                        .Operation = *operation,
                        .Method = *method,
                        .Path = std::move(request_path),
                        .Query = std::move(request_query),
                        .Headers = std::move(headers),
                        .Body = std::move(request_body),
                    },
                .Response =
                    {
                        .Status = 200,
                        .Headers = response_headers(),
                        .Body = std::move(response_body),
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
    const github_public_path_fixture fixture,
    const deterministic_git_fixture& git)
    -> std::vector<recorded_exchange>
{
    auto exchanges = load_recording(fixture, git, git.HeadOne);
    exchanges.resize(1U);
    return exchanges;
}

struct fixture_recording_state
{
    fixture_recording_state(
        const github_public_path_fixture selected_fixture,
        const adapters::github::validated_github_host_profile& selected_profile,
        std::shared_ptr<deterministic_git_fixture> git,
        const github_epoch_fixture_phase phase)
        : Profile{selected_profile}
        , Git{std::move(git)}
        , BootstrapRecording{load_bootstrap_recording(selected_fixture, *Git)}
        , Recording{[&]
              {
                  auto result = load_recording(
                      selected_fixture,
                      *Git,
                      phase == github_epoch_fixture_phase::reuse
                          ? std::string_view{Git->HeadTwo}
                          : std::string_view{Git->HeadOne});
                  if(phase == github_epoch_fixture_phase::seed)
                  {
                      auto second = load_recording(
                          selected_fixture, *Git, Git->HeadTwo);
                      if(second.empty())
                      {
                          throw std::runtime_error{
                              "GitHub fixture refresh recording is empty"};
                      }
                      second.erase(second.begin());
                      result.insert(result.end(),
                                    std::make_move_iterator(
                                        second.begin()),
                                    std::make_move_iterator(second.end()));
                  }
                  return result;
              }()}
        , CapabilityBody{load_capability_response(selected_fixture)}
    {
    }

    adapters::github::validated_github_host_profile Profile;
    std::shared_ptr<deterministic_git_fixture> Git;
    strict_recorded_transport BootstrapRecording;
    strict_recorded_transport Recording;
    std::string CapabilityBody;
    std::size_t CapabilityCalls = 0U;
    std::size_t BootstrapFactoryCalls = 0U;
    std::size_t AccountFactoryCalls = 0U;
    std::size_t AccountReadCalls = 0U;
    std::size_t SmartAcquireCalls = 0U;
    std::size_t SmartDiscoveryCalls = 0U;
    std::size_t SmartLsRefsCalls = 0U;
    std::size_t SmartFetchCalls = 0U;
    int LastReadOperation = -1;
    int RecordingFailure = -1;
    mutable std::mutex Mutex;
};

enum class fixture_transport_role
{
    bootstrap,
    account,
};

[[nodiscard]] auto byte_vector(const std::string_view value)
    -> std::vector<std::byte>
{
    return {
        reinterpret_cast<const std::byte*>(value.data()),
        reinterpret_cast<const std::byte*>(value.data() + value.size()),
    };
}

void append_packet(std::vector<std::byte>& output,
                   const std::span<const std::byte> payload)
{
    constexpr std::string_view digits = "0123456789abcdef";
    const auto total = payload.size() + 4U;
    if(total > 0xffffU)
    {
        throw std::runtime_error{"Git fixture packet is too large"};
    }
    auto header = std::array<char, 4U>{};
    auto remaining = total;
    for(auto index = header.size(); index-- > 0U;)
    {
        header[index] = digits[remaining & 0x0fU];
        remaining >>= 4U;
    }
    const auto header_bytes = std::as_bytes(std::span{header});
    output.insert(output.end(), header_bytes.begin(), header_bytes.end());
    output.insert(output.end(), payload.begin(), payload.end());
}

void append_packet(std::vector<std::byte>& output,
                   const std::string_view payload)
{
    append_packet(output, std::as_bytes(std::span{payload}));
}

void append_flush(std::vector<std::byte>& output)
{
    const auto flush = byte_vector("0000");
    output.insert(output.end(), flush.begin(), flush.end());
}

[[nodiscard]] auto discovery_body() -> std::vector<std::byte>
{
    auto result = std::vector<std::byte>{};
    append_packet(result, "version 2\n");
    append_packet(result, "ls-refs=unborn\n");
    append_packet(result, "fetch=shallow wait-for-done\n");
    append_packet(result, "object-format=sha1\n");
    append_flush(result);
    return result;
}

[[nodiscard]] auto refs_body(
    const application::git_smart_request_context& context)
    -> std::vector<std::byte>
{
    auto result = std::vector<std::byte>{};
    append_packet(
        result,
        std::string{core::canonical_hex(context.Revision.Base).view()}
            + " " + std::string{context.BaseRef.view()} + "\n");
    append_packet(
        result,
        std::string{core::canonical_hex(context.Revision.Head).view()}
            + " refs/pull/" + std::to_string(context.Authority.Number.value())
            + "/head\n");
    append_flush(result);
    return result;
}

[[nodiscard]] auto fetch_body(const QByteArray& pack)
    -> std::vector<std::byte>
{
    auto result = std::vector<std::byte>{};
    append_packet(result, "packfile\n");
    constexpr auto maximum_chunk = std::size_t{60U * 1024U};
    const auto pack_bytes = std::span{
        reinterpret_cast<const std::byte*>(pack.constData()),
        static_cast<std::size_t>(pack.size())};
    for(auto offset = std::size_t{0U}; offset < pack_bytes.size();)
    {
        const auto size = std::min(maximum_chunk, pack_bytes.size() - offset);
        auto payload = std::vector<std::byte>{};
        payload.reserve(size + 1U);
        payload.push_back(std::byte{1U});
        payload.insert(payload.end(),
                       pack_bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                       pack_bytes.begin()
                           + static_cast<std::ptrdiff_t>(offset + size));
        append_packet(result, payload);
        offset += size;
    }
    append_flush(result);
    return result;
}

class fixture_smart_transport final
    : public application::git_smart_transport_port
{
public:
    fixture_smart_transport(
        std::shared_ptr<fixture_recording_state> state,
        application::git_smart_request_context context)
        : State{std::move(state)}, Context{std::move(context)}
    {
    }

    void revoke() noexcept override { Revoked.store(true); }

    [[nodiscard]] auto revoked() const noexcept -> bool override
    {
        return Revoked.load();
    }

    [[nodiscard]] auto discover(
        const application::git_smart_discovery_request& request,
        const std::stop_token stop)
        -> application::git_smart_transport_result<
            application::git_smart_discovery_response> override
    {
        if(stop.stop_requested() || revoked())
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled}};
        }
        if(request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    invalid_request}};
        }
        {
            const std::scoped_lock lock{State->Mutex};
            ++State->SmartDiscoveryCalls;
        }
        return application::git_smart_discovery_response{
            .Body = discovery_body()};
    }

    [[nodiscard]] auto ls_refs(
        const application::git_smart_ls_refs_request& request,
        const std::stop_token stop)
        -> application::git_smart_transport_result<
            application::git_smart_ls_refs_response> override
    {
        if(stop.stop_requested() || revoked())
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled}};
        }
        if(request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    invalid_request}};
        }
        {
            const std::scoped_lock lock{State->Mutex};
            ++State->SmartLsRefsCalls;
        }
        return application::git_smart_ls_refs_response{
            .Body = refs_body(Context)};
    }

    [[nodiscard]] auto fetch(
        const application::git_smart_fetch_request& request,
        application::git_smart_wire_consumer& consumer,
        const std::stop_token stop)
        -> application::git_smart_transport_result<void> override
    {
        if(stop.stop_requested() || revoked())
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled}};
        }
        if(request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    invalid_request}};
        }
        const auto head = std::string{
            core::canonical_hex(Context.Revision.Head).view()};
        const auto& pack = head == State->Git->HeadOne
            ? State->Git->PackOne
            : head == State->Git->HeadTwo ? State->Git->PackTwo
                                          : QByteArray{};
        if(pack.isEmpty())
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    invalid_request}};
        }
        const auto body = fetch_body(pack);
        if(!consumer.consume(body))
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    consumer_rejected}};
        }
        {
            const std::scoped_lock lock{State->Mutex};
            ++State->SmartFetchCalls;
        }
        return {};
    }

private:
    std::shared_ptr<fixture_recording_state> State;
    application::git_smart_request_context Context;
    std::atomic_bool Revoked = false;
};

class fixture_recorded_wire_transport final
    : public github_wire_transport_port,
      public application::git_smart_transport_factory_port
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

    [[nodiscard]] auto acquire(
        const application::git_smart_request_context& context)
        -> application::git_smart_transport_result<
            application::git_smart_transport> override
    {
        const auto base =
            std::string{core::canonical_hex(context.Revision.Base).view()};
        const auto head =
            std::string{core::canonical_hex(context.Revision.Head).view()};
        if(Role != fixture_transport_role::account
           || !application::valid(context)
           || context.Authority.Account != core::github_account_id{1001U}
           || context.Authority.Repository != core::repository_id{2001U}
           || context.Authority.PullRequest != core::pull_request_id{3001U}
           || context.Authority.Number != core::pull_request_number{17U}
           || context.Repository
                  != core::repository_locator{
                      .Owner = "octo-org",
                      .Name = "review-repository",
                  }
           || context.BaseRef.view() != "refs/heads/main"
           || base != State->Git->Base
           || (head != State->Git->HeadOne && head != State->Git->HeadTwo))
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    invalid_request}};
        }
        {
            const std::scoped_lock lock{State->Mutex};
            ++State->SmartAcquireCalls;
        }
        return application::git_smart_transport{
            new fixture_smart_transport{State, context}};
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
        application::effect_executor& vault_effects,
        github_public_path_fixture_options options)
    {
        GitExecutable = QFileInfo{
            QStandardPaths::findExecutable(QStringLiteral("git"))}
                            .canonicalFilePath()
                            .toStdString();
        if(GitExecutable.empty())
        {
            throw std::runtime_error{
                "GitHub public-path installed Git is unavailable"};
        }
        Git = std::make_shared<deterministic_git_fixture>();
        Recording = std::make_shared<fixture_recording_state>(
            fixture, profile, Git, options.EpochPhase);
        Factory = std::make_shared<fixture_transport_factory>(Recording);
        VaultState = std::make_shared<fixture_vault_state>();
        Vault = std::make_unique<fixture_credential_vault>(
            vault_effects, VaultState);

        if(options.ApplicationDataRoot)
        {
            ApplicationDataRoot =
                options.ApplicationDataRoot->lexically_normal();
            std::error_code error;
            static_cast<void>(std::filesystem::create_directories(
                ApplicationDataRoot, error));
            std::filesystem::permissions(
                ApplicationDataRoot,
                std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace,
                error);
            if(error)
            {
                throw std::runtime_error{
                    "GitHub public-path application data is unavailable"};
            }
            ApplicationDataRoot =
                std::filesystem::canonical(ApplicationDataRoot, error);
            if(error)
            {
                throw std::runtime_error{
                    "GitHub public-path application data is unavailable"};
            }
            return;
        }

        ApplicationData = std::make_unique<QTemporaryDir>(
            QStringLiteral("bpr-public-path-XXXXXX"));
        if(!ApplicationData->isValid())
        {
            throw std::runtime_error{
                "GitHub public-path application data is unavailable"};
        }
        ApplicationDataRoot =
            QFileInfo{ApplicationData->path()}.canonicalFilePath().toStdString();
        if(ApplicationDataRoot.empty())
        {
            throw std::runtime_error{
                "GitHub public-path installed Git is unavailable"};
        }
    }

    std::shared_ptr<fixture_recording_state> Recording;
    std::shared_ptr<deterministic_git_fixture> Git;
    std::shared_ptr<fixture_transport_factory> Factory;
    std::shared_ptr<fixture_vault_state> VaultState;
    std::unique_ptr<fixture_credential_vault> Vault;
    std::unique_ptr<QTemporaryDir> ApplicationData;
    std::filesystem::path ApplicationDataRoot;
    std::filesystem::path GitExecutable;
};

github_public_path_fixture_components::
    github_public_path_fixture_components(
        const github_public_path_fixture fixture,
        const adapters::github::validated_github_host_profile& profile,
        application::effect_executor& vault_effects,
        github_public_path_fixture_options options)
    : Implementation{std::make_unique<implementation>(
          fixture, profile, vault_effects, std::move(options))}
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
        result.SmartAcquireCount = static_cast<int>(
            Implementation->Recording->SmartAcquireCalls);
        result.SmartDiscoveryCount = static_cast<int>(
            Implementation->Recording->SmartDiscoveryCalls);
        result.SmartLsRefsCount = static_cast<int>(
            Implementation->Recording->SmartLsRefsCalls);
        result.SmartFetchCount = static_cast<int>(
            Implementation->Recording->SmartFetchCalls);
        result.BaseOid = Implementation->Git->Base;
        result.HeadOneOid = Implementation->Git->HeadOne;
        result.HeadTwoOid = Implementation->Git->HeadTwo;
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

auto github_public_path_fixture_components::application_data_root() const
    -> const std::filesystem::path&
{
    return Implementation->ApplicationDataRoot;
}

auto github_public_path_fixture_components::git_executable() const
    -> const std::filesystem::path&
{
    return Implementation->GitExecutable;
}
}
