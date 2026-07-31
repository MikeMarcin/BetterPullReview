#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr;

static_assert(
    !std::is_copy_constructible_v<application::git_smart_transport_port>);
static_assert(
    !std::is_move_constructible_v<application::git_smart_transport_port>);
static_assert(
    !std::is_copy_constructible_v<
        application::git_smart_transport_factory_port>);
static_assert(
    !std::is_move_constructible_v<
        application::git_smart_transport_factory_port>);

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32U> bytes{};
    bytes.front() = seed;
    bytes.back() = static_cast<std::uint8_t>(seed ^ 0xa5U);
    return Identity{bytes};
}

[[nodiscard]] auto bytes(const std::string_view value)
    -> std::vector<std::byte>
{
    return {
        reinterpret_cast<const std::byte*>(value.data()),
        reinterpret_cast<const std::byte*>(value.data() + value.size()),
    };
}

[[nodiscard]] auto text(const std::span<const std::byte> value) -> std::string
{
    return {
        reinterpret_cast<const char*>(value.data()),
        value.size(),
    };
}

[[nodiscard]] auto packet(const std::string_view payload) -> std::string
{
    constexpr std::string_view digits = "0123456789abcdef";
    const auto total = payload.size() + 4U;
    std::array<char, 4U> header{};
    auto remaining = total;
    for(auto index = header.size(); index-- > 0U;)
    {
        header[index] = digits[remaining & 0x0fU];
        remaining >>= 4U;
    }
    return std::string{header.data(), header.size()} + std::string{payload};
}

[[nodiscard]] auto oid(
    const char digit,
    const core::git_hash_algorithm format) -> core::git_commit_oid
{
    const auto length =
        format == core::git_hash_algorithm::sha1 ? 40U : 64U;
    const auto parsed =
        core::parse_git_commit_oid(std::string(length, digit));
    expect(parsed.has_value(), "test oid is canonical");
    return *parsed;
}

[[nodiscard]] auto authority() -> core::pull_request_authority
{
    return {
        .Profile = core::host_profile_id{11U},
        .ProfileRevision = core::github_profile_revision{12U},
        .Host = core::git_host_id{13U},
        .HostAuthority = opaque<core::host_authority_fingerprint>(14U),
        .Account = core::github_account_id{15U},
        .Viewer = core::github_viewer_actor_id{16U},
        .Repository = core::repository_id{17U},
        .Cache = opaque<core::repository_cache_scope_id>(18U),
        .PullRequest = core::pull_request_id{19U},
        .Number = core::pull_request_number{81U},
        .Scope = opaque<core::pull_request_authority_scope_id>(20U),
    };
}

[[nodiscard]] auto context(
    const core::git_hash_algorithm format =
        core::git_hash_algorithm::sha1)
    -> application::git_smart_request_context
{
    const auto base_ref =
        application::parse_git_base_branch_ref("refs/heads/main");
    expect(base_ref.has_value(), "test base ref is valid");
    return {
        .ApplicationSession = core::application_session_id{1U},
        .Authority = authority(),
        .Revision =
            {
                .Source =
                    {
                        .Snapshot = core::provider_snapshot_id{2U},
                        .IdentityGeneration =
                            core::github_identity_generation{3U},
                        .PullRequestGeneration =
                            core::request_generation{4U},
                    },
                .Origin = core::pull_request_revision_origin::live,
                .BaseBranch = opaque<core::base_branch_id>(21U),
                .Base = oid('1', format),
                .Head = oid('2', format),
            },
        .Repository = {.Owner = "openai", .Name = "better-pull-review"},
        .BaseRef = *base_ref,
    };
}

[[nodiscard]] auto capabilities(
    const core::git_hash_algorithm format =
        core::git_hash_algorithm::sha1)
    -> application::detail::git_protocol_v2_capabilities
{
    return {
        .ObjectFormat = format,
        .LsRefs = true,
        .Fetch = true,
    };
}

[[nodiscard]] auto advertisement(
    const core::git_hash_algorithm format) -> std::vector<std::byte>
{
    const auto format_name =
        format == core::git_hash_algorithm::sha1 ? "sha1" : "sha256";
    const auto body = packet("version 2\n") + packet("ls-refs=unborn\n")
        + packet("fetch=shallow wait-for-done\n")
        + packet(std::string{"object-format="} + format_name + "\n")
        + packet("server-option\n") + "0000";
    return bytes(body);
}

[[nodiscard]] auto refs_response(
    const application::git_smart_request_context& request_context)
    -> std::vector<std::byte>
{
    const auto base =
        std::string{core::canonical_hex(request_context.Revision.Base).view()};
    const auto head =
        std::string{core::canonical_hex(request_context.Revision.Head).view()};
    const auto body =
        packet(std::string{base} + " refs/heads/main\n")
        + packet(std::string{head} + " refs/pull/81/head\n") + "0000";
    return bytes(body);
}

class recording_pack_sink final : public application::git_pack_stream_sink
{
public:
    explicit recording_pack_sink(const core::git_hash_algorithm format)
        : Format{format}
    {
    }

    [[nodiscard]] auto append(
        const std::span<const std::byte> input,
        const std::stop_token stop)
        -> application::git_pack_stream_result<void> override
    {
        if(AppendFailure.has_value())
        {
            return std::unexpected(*AppendFailure);
        }
        if(stop.stop_requested())
        {
            return std::unexpected(
                application::git_pack_stream_failure::cancelled);
        }
        Received.insert(Received.end(), input.begin(), input.end());
        return {};
    }

    [[nodiscard]] auto finish(const std::stop_token stop)
        -> application::git_pack_stream_result<
            application::completed_git_pack_receipt> override
    {
        if(FinishFailure.has_value())
        {
            return std::unexpected(*FinishFailure);
        }
        if(stop.stop_requested())
        {
            return std::unexpected(
                application::git_pack_stream_failure::cancelled);
        }
        Finished = true;
        application::git_pack_oid pack{
            .Algorithm = Format,
        };
        pack.Digest.front() = 0x42U;
        auto observed = pack;
        if(MismatchedReceipt)
        {
            observed.Digest.front() = 0x43U;
        }
        auto token = std::array<std::uint8_t, 32U>{};
        token.front() = 0x71U;
        auto spool_identity =
            application::git_artifact_identity{};
        spool_identity.Bytes.front() = 1U;
        spool_identity.Bytes[1U] = 1U;
        spool_identity.Bytes[2U] = 0x7fU;
        spool_identity.Bytes.back() = 0x51U;
        return application::completed_git_pack_receipt{
            .Operation = opaque<core::epoch_operation_id>(41U),
            .ExpectedPack = pack,
            .ObservedPack = observed,
            .Spool =
                {
                    .Role =
                        application::git_artifact_role::
                            completed_spool,
                    .Identity = spool_identity,
                    .Bytes = Received.size(),
                },
            .OpaqueToken = token,
        };
    }

    auto abort() noexcept -> void override
    {
        Aborted = true;
        ++AbortCalls;
    }

    core::git_hash_algorithm Format;
    std::vector<std::byte> Received;
    std::optional<application::git_pack_stream_failure> AppendFailure;
    std::optional<application::git_pack_stream_failure> FinishFailure;
    bool MismatchedReceipt = false;
    bool Finished = false;
    bool Aborted = false;
    std::size_t AbortCalls = 0U;
};

[[nodiscard]] auto fetch_response() -> std::vector<std::byte>
{
    std::string progress(1U, static_cast<char>(2));
    progress += "counting\n";
    std::string pack_data(1U, static_cast<char>(1));
    pack_data += "PACKdata";
    return bytes(
        packet("packfile\n") + packet(progress) + packet(pack_data)
        + "0000");
}

[[nodiscard]] constexpr auto fetch_limits()
    -> application::git_protocol_v2_fetch_limits
{
    return {
        .MaximumWireBytes = 4096U,
        .MaximumPackBytes = 1024U,
        .MaximumProgressBytes = 128U,
        .MaximumPackets = 16U,
    };
}

class scripted_transport final
    : public application::git_smart_transport_port
{
public:
    explicit scripted_transport(
        application::git_smart_request_context bound_context)
        : BoundContext{std::move(bound_context)}
    {
        DiscoveryBody = advertisement(
            BoundContext.Revision.Base.algorithm());
        LsRefsBody = refs_response(BoundContext);
        FetchBody = fetch_response();
    }

    auto revoke() noexcept -> void override
    {
        Revoked = true;
    }

    [[nodiscard]] auto revoked() const noexcept -> bool override
    {
        return Revoked;
    }

    [[nodiscard]] auto discover(
        const application::git_smart_discovery_request& request,
        std::stop_token)
        -> application::git_smart_transport_result<
            application::git_smart_discovery_response> override
    {
        Calls.emplace_back("discover");
        if(Revoked || request.Context != BoundContext)
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure =
                    application::git_smart_transport_failure::invalid_request,
            });
        }
        if(DiscoveryFailure.has_value())
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure = *DiscoveryFailure,
            });
        }
        return application::git_smart_discovery_response{
            .Body = DiscoveryBody,
        };
    }

    [[nodiscard]] auto ls_refs(
        const application::git_smart_ls_refs_request& request,
        std::stop_token)
        -> application::git_smart_transport_result<
            application::git_smart_ls_refs_response> override
    {
        Calls.emplace_back("ls-refs");
        if(
            Revoked || request.Context != BoundContext
            || !application::valid(request))
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure =
                    application::git_smart_transport_failure::invalid_request,
            });
        }
        CapturedLsRefsBody.assign(request.Body.begin(), request.Body.end());
        if(LsRefsFailure.has_value())
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure = *LsRefsFailure,
            });
        }
        return application::git_smart_ls_refs_response{
            .Body = LsRefsBody,
        };
    }

    [[nodiscard]] auto fetch(
        const application::git_smart_fetch_request& request,
        application::git_smart_wire_consumer& consumer,
        std::stop_token)
        -> application::git_smart_transport_result<void> override
    {
        Calls.emplace_back("fetch");
        if(
            Revoked || request.Context != BoundContext
            || !application::valid(request))
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure =
                    application::git_smart_transport_failure::invalid_request,
            });
        }
        CapturedFetchBody.assign(request.Body.begin(), request.Body.end());
        CapturedMaximumResponseBytes = request.MaximumResponseBytes;
        if(FetchFailure.has_value())
        {
            return std::unexpected(application::git_smart_transport_error{
                .Failure = *FetchFailure,
            });
        }

        const auto chunk_size = std::max<std::size_t>(ChunkSize, 1U);
        std::size_t cursor = 0U;
        while(cursor < FetchBody.size())
        {
            const auto count =
                std::min(chunk_size, FetchBody.size() - cursor);
            if(!consumer.consume(
                   std::span<const std::byte>{
                       FetchBody.data() + cursor,
                       count,
                   }))
            {
                return std::unexpected(
                    application::git_smart_transport_error{
                        .Failure = application::git_smart_transport_failure::
                            consumer_rejected,
                    });
            }
            cursor += count;
        }
        return {};
    }

    application::git_smart_request_context BoundContext;
    std::vector<std::byte> DiscoveryBody;
    std::vector<std::byte> LsRefsBody;
    std::vector<std::byte> FetchBody;
    std::vector<std::byte> CapturedLsRefsBody;
    std::vector<std::byte> CapturedFetchBody;
    std::optional<application::git_smart_transport_failure>
        DiscoveryFailure;
    std::optional<application::git_smart_transport_failure> LsRefsFailure;
    std::optional<application::git_smart_transport_failure> FetchFailure;
    std::uint64_t CapturedMaximumResponseBytes = 0U;
    std::size_t ChunkSize = 1U;
    bool Revoked = false;
    std::vector<std::string_view> Calls;
};

auto test_base_ref_and_context_validation() -> void
{
    expect(
        application::parse_git_base_branch_ref("refs/heads/topic/name")
            .has_value(),
        "a canonical full heads ref is accepted");
    for(const auto invalid : {
            "main",
            "refs/heads/",
            "refs/heads/.hidden",
            "refs/heads/topic..name",
            "refs/heads/topic.lock",
            "refs/heads/topic name",
            "refs/heads/topic@{one",
        })
    {
        expect(
            !application::parse_git_base_branch_ref(invalid).has_value(),
            "noncanonical base ref is rejected");
    }

    auto valid_context = context();
    expect(application::valid(valid_context), "complete context is valid");
    valid_context.ApplicationSession = {};
    expect(
        !application::valid(valid_context),
        "zero application session invalidates context");
}

auto test_advertisement_and_requests() -> void
{
    for(const auto format : {
            core::git_hash_algorithm::sha1,
            core::git_hash_algorithm::sha256,
        })
    {
        const auto parsed =
            application::detail::parse_git_protocol_v2_advertisement(
            advertisement(format),
            format);
        expect(parsed.has_value(), "strict v2 advertisement is accepted");
        expect(
            parsed->ObjectFormat == format && parsed->LsRefs
                && parsed->Fetch,
            "required capabilities and object format are retained");
    }

    const auto mismatched =
        application::detail::parse_git_protocol_v2_advertisement(
        advertisement(core::git_hash_algorithm::sha256),
        core::git_hash_algorithm::sha1);
    expect(
        !mismatched.has_value()
            && mismatched.error().Failure
                == application::git_protocol_v2_failure::
                    object_format_mismatch,
        "object-format mismatch is rejected");

    const auto missing_fetch = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet("object-format=sha1\n") + "0000");
    const auto missing_fetch_result =
        application::detail::parse_git_protocol_v2_advertisement(
            missing_fetch,
            core::git_hash_algorithm::sha1);
    expect(
        !missing_fetch_result.has_value()
            && missing_fetch_result.error().Failure
                == application::git_protocol_v2_failure::
                    missing_capability,
        "a missing required command capability is rejected");

    const auto missing_format = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet("fetch\n") + "0000");
    const auto missing_format_result =
        application::detail::parse_git_protocol_v2_advertisement(
            missing_format,
            core::git_hash_algorithm::sha1);
    expect(
        !missing_format_result.has_value()
            && missing_format_result.error().Failure
                == application::git_protocol_v2_failure::
                    missing_capability,
        "a missing object-format capability is rejected");

    const auto duplicate = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet("fetch\n") + packet("fetch=shallow\n")
        + packet("object-format=sha1\n") + "0000");
    const auto duplicate_result =
        application::detail::parse_git_protocol_v2_advertisement(
            duplicate,
            core::git_hash_algorithm::sha1);
    expect(
        !duplicate_result.has_value()
            && duplicate_result.error().Failure
                == application::git_protocol_v2_failure::malformed_packet,
        "a duplicate capability key is rejected");

    const auto malformed = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet("fetch\n") + packet("bad key=value\n")
        + packet("object-format=sha1\n") + "0000");
    const auto malformed_result =
        application::detail::parse_git_protocol_v2_advertisement(
            malformed,
            core::git_hash_algorithm::sha1);
    expect(
        !malformed_result.has_value()
            && malformed_result.error().Failure
                == application::git_protocol_v2_failure::malformed_packet,
        "a malformed capability line is rejected");

    const auto malformed_value = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet("fetch\n") + packet("extension=bad|value\n")
        + packet("object-format=sha1\n") + "0000");
    expect(
        !application::detail::parse_git_protocol_v2_advertisement(
             malformed_value,
             core::git_hash_algorithm::sha1)
             .has_value(),
        "unknown capabilities with out-of-grammar values are rejected");

    const auto no_lf = bytes(
        packet("version 2") + packet("ls-refs")
        + packet("fetch=shallow wait-for-done")
        + packet("object-format=sha1") + "0000");
    expect(
        application::detail::parse_git_protocol_v2_advertisement(
            no_lf,
            core::git_hash_algorithm::sha1)
            .has_value(),
        "non-binary pkt-lines are accepted without their optional LF");

    const auto carriage_return = bytes(
        packet("version 2\n") + packet("ls-refs\r\n")
        + packet("fetch\n") + packet("object-format=sha1\n") + "0000");
    expect(
        !application::detail::parse_git_protocol_v2_advertisement(
             carriage_return,
             core::git_hash_algorithm::sha1)
             .has_value(),
        "carriage returns remain invalid when the optional LF is normalized");

    auto nul_capability = std::string{"fetch"};
    nul_capability.push_back('\0');
    nul_capability += "ignored\n";
    const auto embedded_nul = bytes(
        packet("version 2\n") + packet("ls-refs\n")
        + packet(nul_capability) + packet("object-format=sha1\n") + "0000");
    expect(
        !application::detail::parse_git_protocol_v2_advertisement(
             embedded_nul,
             core::git_hash_algorithm::sha1)
             .has_value(),
        "embedded NUL remains invalid when optional LF is normalized");

    const auto request_context = context();
    const auto caps = capabilities();
    const auto ls_request =
        application::detail::make_git_protocol_v2_ls_refs_request(
            request_context,
            caps);
    const auto expected_ls =
        packet("command=ls-refs\n") + packet("object-format=sha1\n")
        + "0001" + packet("ref-prefix refs/heads/main\n")
        + packet("ref-prefix refs/pull/81/head\n") + "0000";
    expect(
        ls_request.has_value() && text(*ls_request) == expected_ls,
        "ls-refs request has only the exact command, format, and two refs");

    const auto have_oid = oid('3', core::git_hash_algorithm::sha1);
    const std::array haves{
        application::git_protocol_v2_have{
            .Cache = request_context.Authority.Cache,
            .Commit = have_oid,
            .CatalogVisible = true,
            .ObjectVerified = true,
        },
        application::git_protocol_v2_have{
            .Cache = request_context.Authority.Cache,
            .Commit = have_oid,
            .CatalogVisible = true,
            .ObjectVerified = true,
        },
    };
    const auto fetch_request =
        application::detail::make_git_protocol_v2_fetch_request(
            request_context,
            caps,
            haves);
    const auto base =
        std::string{core::canonical_hex(request_context.Revision.Base).view()};
    const auto head =
        std::string{core::canonical_hex(request_context.Revision.Head).view()};
    const auto have = std::string{core::canonical_hex(have_oid).view()};
    const auto expected_fetch =
        packet("command=fetch\n") + packet("object-format=sha1\n")
        + "0001" + packet(std::string{"want "} + std::string{base} + "\n")
        + packet(std::string{"want "} + std::string{head} + "\n")
        + packet(std::string{"have "} + std::string{have} + "\n")
        + packet("done\n") + "0000";
    expect(
        fetch_request.has_value() && text(*fetch_request) == expected_fetch,
        "fetch request deduplicates verified same-cache haves");
    expect(
        text(*fetch_request).find("thin-pack") == std::string::npos
            && text(*fetch_request).find("sideband-all") == std::string::npos
            && text(*fetch_request).find("filter") == std::string::npos,
        "fetch request omits alternate response capabilities");

    auto wrong_cache = haves;
    wrong_cache.front().Cache =
        opaque<core::repository_cache_scope_id>(99U);
    expect(
        !application::detail::make_git_protocol_v2_fetch_request(
             request_context,
             caps,
             wrong_cache)
             .has_value(),
        "a have from another cache is rejected");
    auto unverified = haves;
    unverified.front().ObjectVerified = false;
    expect(
        !application::detail::make_git_protocol_v2_fetch_request(
             request_context,
             caps,
             unverified)
             .has_value(),
        "an unverified have is rejected");
    const std::vector excessive_haves(
        application::maximum_git_protocol_v2_haves + 1U,
        haves.front());
    expect(
        !application::detail::make_git_protocol_v2_fetch_request(
             request_context,
             caps,
             excessive_haves)
             .has_value(),
        "the candidate have set is bounded before deduplication");

    const auto sha256_context = context(core::git_hash_algorithm::sha256);
    const auto sha256_request =
        application::detail::make_git_protocol_v2_fetch_request(
            sha256_context,
            capabilities(core::git_hash_algorithm::sha256),
            {});
    expect(
        sha256_request.has_value()
            && text(*sha256_request).find("object-format=sha256\n")
                != std::string::npos
            && text(*sha256_request).find(
                   std::string{
                       core::canonical_hex(sha256_context.Revision.Base)
                           .view()})
                != std::string::npos,
        "SHA-256 requests echo SHA-256 and render full 64-digit wants");

    auto identical_wants = request_context;
    identical_wants.Revision.Head = identical_wants.Revision.Base;
    const auto deduplicated_wants =
        application::detail::make_git_protocol_v2_fetch_request(
            identical_wants,
            caps,
            {});
    expect(deduplicated_wants.has_value(), "equal accepted wants are valid");
    const auto wants_text = text(*deduplicated_wants);
    const auto first_want = wants_text.find("want ");
    expect(
        first_want != std::string::npos
            && wants_text.find("want ", first_want + 1U)
                == std::string::npos,
        "equal base and head wants are emitted once");
}

auto test_ls_refs_strict_identity() -> void
{
    const auto request_context = context();
    const auto parsed = application::detail::parse_git_protocol_v2_ls_refs(
        refs_response(request_context),
        request_context);
    expect(
        parsed.has_value()
            && parsed->Base == request_context.Revision.Base
            && parsed->Head == request_context.Revision.Head,
        "ls-refs retains the exact accepted base and PR head");

    const auto base =
        std::string{core::canonical_hex(request_context.Revision.Base).view()};
    const auto head =
        std::string{core::canonical_hex(request_context.Revision.Head).view()};
    const auto extra = bytes(
        packet(std::string{base} + " refs/heads/main\n")
        + packet(std::string{head} + " refs/pull/81/head\n")
        + packet(std::string{head} + " refs/heads/other\n") + "0000");
    const auto rejected = application::detail::parse_git_protocol_v2_ls_refs(
        extra,
        request_context);
    expect(
        !rejected.has_value()
            && rejected.error().Failure
                == application::git_protocol_v2_failure::unexpected_ref,
        "an unrequested advertised ref is rejected");

    const auto attributed = bytes(
        packet(std::string{base} + " refs/heads/main peeled:"
               + std::string{base} + "\n")
        + packet(std::string{head} + " refs/pull/81/head\n") + "0000");
    expect(
        !application::detail::parse_git_protocol_v2_ls_refs(
             attributed,
             request_context)
             .has_value(),
        "unrequested ls-refs attributes are rejected");

    const auto duplicate = bytes(
        packet(std::string{base} + " refs/heads/main\n")
        + packet(std::string{base} + " refs/heads/main\n")
        + packet(std::string{head} + " refs/pull/81/head\n") + "0000");
    const auto duplicate_result =
        application::detail::parse_git_protocol_v2_ls_refs(
            duplicate,
            request_context);
    expect(
        !duplicate_result.has_value()
            && duplicate_result.error().Failure
                == application::git_protocol_v2_failure::duplicate_ref,
        "a duplicate requested ref is rejected");

    const auto missing = bytes(
        packet(std::string{base} + " refs/heads/main\n") + "0000");
    const auto missing_result =
        application::detail::parse_git_protocol_v2_ls_refs(
        missing,
        request_context);
    expect(
        !missing_result.has_value()
            && missing_result.error().Failure
                == application::git_protocol_v2_failure::
                    incomplete_response,
        "a missing requested ref is rejected");

    const auto wrong_oid = std::string{
        core::canonical_hex(
            oid('3', core::git_hash_algorithm::sha1))
            .view()};
    const auto mismatched = bytes(
        packet(std::string{wrong_oid} + " refs/heads/main\n")
        + packet(std::string{head} + " refs/pull/81/head\n") + "0000");
    const auto mismatch_result =
        application::detail::parse_git_protocol_v2_ls_refs(
            mismatched,
            request_context);
    expect(
        !mismatch_result.has_value()
            && mismatch_result.error().Failure
                == application::git_protocol_v2_failure::oid_mismatch,
        "an advertised ref at the wrong accepted oid is rejected");

    const auto no_lf = bytes(
        packet(std::string{base} + " refs/heads/main")
        + packet(std::string{head} + " refs/pull/81/head") + "0000");
    expect(
        application::detail::parse_git_protocol_v2_ls_refs(
            no_lf,
            request_context)
            .has_value(),
        "ls-refs text packets are accepted without their optional LF");
}

auto test_incremental_fetch_parser() -> void
{
    recording_pack_sink sink{core::git_hash_algorithm::sha1};
    application::detail::git_protocol_v2_fetch_parser parser{
        sink,
        fetch_limits(),
        core::git_hash_algorithm::sha1,
        {},
    };
    const auto response = fetch_response();
    std::size_t offset = 0U;
    for(const auto byte : response)
    {
        if(!parser.consume(std::span<const std::byte>{&byte, 1U}))
        {
            std::cerr << "fetch parser failed at byte " << offset
                      << " with error "
                      << static_cast<int>(parser.error()->Failure) << '\n';
            expect(false, "every one-byte raw transport chunk is consumed");
        }
        ++offset;
    }
    const auto result = parser.finish();
    expect(result.has_value(), "complete sideband response finishes");
    expect(
        text(sink.Received) == "PACKdata",
        "only sideband channel one reaches the pack sink");
    expect(
        result->PackBytes == 8U && result->ProgressBytes == 9U
            && result->WireBytes == response.size()
            && result->Packets == 4U && sink.Finished && !sink.Aborted,
        "bounded parser reports exact retained counters");

    recording_pack_sink no_lf_sink{core::git_hash_algorithm::sha1};
    application::detail::git_protocol_v2_fetch_parser no_lf_parser{
        no_lf_sink,
        fetch_limits(),
        core::git_hash_algorithm::sha1,
        {},
    };
    std::string pack_data(1U, static_cast<char>(1));
    pack_data += "PACKdata";
    const auto no_lf_response =
        bytes(packet("packfile") + packet(pack_data) + "0000");
    for(const auto byte : no_lf_response)
    {
        expect(
            no_lf_parser.consume(
                std::span<const std::byte>{&byte, 1U}),
            "a no-LF section remains valid across one-byte fragments");
    }
    expect(
        no_lf_parser.finish().has_value()
            && text(no_lf_sink.Received) == "PACKdata"
            && no_lf_sink.AbortCalls == 0U,
        "the packfile section header accepts an omitted optional LF");
}

auto test_fetch_parser_rejections() -> void
{
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        const auto response = bytes(
            packet("acknowledgments\n") + packet("NAK\n") + "0000");
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::
                        unexpected_section
                && sink.Aborted,
            "an acknowledgments section is rejected before pack input");
    }
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        std::string fatal(1U, static_cast<char>(3));
        fatal += "server failure\n";
        const auto response =
            bytes(packet("packfile\n") + packet(fatal) + "0000");
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::sideband_error
                && sink.Aborted,
            "sideband fatal channel aborts the pack sink");
    }
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        auto response = fetch_response();
        response.resize(response.size() - 4U);
        expect(parser.consume(response), "unterminated body can be streamed");
        const auto result = parser.finish();
        expect(
            !result.has_value()
                && result.error().Failure
                    == application::git_protocol_v2_failure::
                        incomplete_response
                && sink.Aborted,
            "missing terminal flush is rejected");
    }
}

auto test_fetch_limits_and_framing() -> void
{
    const auto response = fetch_response();
    {
        auto limits = fetch_limits();
        limits.MaximumPackBytes = 4U;
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            limits,
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::pack_limit
                && sink.Aborted,
            "pack bytes are bounded before the sink accepts excess data");
    }
    {
        auto limits = fetch_limits();
        limits.MaximumProgressBytes = 4U;
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            limits,
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::progress_limit
                && sink.Aborted,
            "progress bytes are independently bounded");
    }
    {
        auto limits = fetch_limits();
        limits.MaximumWireBytes = response.size() - 1U;
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            limits,
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::response_limit
                && sink.Aborted,
            "raw response bytes are bounded before parsing");
    }
    {
        auto limits = fetch_limits();
        limits.MaximumPackets = 2U;
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            limits,
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(response)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::response_limit
                && sink.Aborted,
            "pkt-line count is bounded");
    }
    {
        std::string invalid_channel(1U, static_cast<char>(4));
        invalid_channel += "data";
        const auto body = bytes(
            packet("packfile\n") + packet(invalid_channel) + "0000");
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(body)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::
                        malformed_packet
                && sink.Aborted,
            "an unknown sideband channel is rejected");
    }
    for(const auto framing : {"zzzz", "0003", "ffff"})
    {
        const auto body = bytes(framing);
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(body)
                && parser.error()->Failure
                    == application::git_protocol_v2_failure::
                        malformed_packet
                && sink.Aborted,
            "invalid pkt-line framing is rejected");
    }
}

auto test_pack_sink_failures_and_cancellation() -> void
{
    const auto response = fetch_response();
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        sink.AppendFailure =
            application::git_pack_stream_failure::storage;
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(
            !parser.consume(response) && parser.error()->PackFailure.has_value()
                && *parser.error()->PackFailure
                    == application::git_pack_stream_failure::storage
                && sink.Aborted,
            "pack append failure is retained and aborts the sink");
    }
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        sink.FinishFailure =
            application::git_pack_stream_failure::malformed_pack;
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(parser.consume(response), "pack reaches finish boundary");
        const auto result = parser.finish();
        expect(
            !result.has_value() && result.error().PackFailure.has_value()
                && *result.error().PackFailure
                    == application::git_pack_stream_failure::malformed_pack
                && sink.Aborted,
            "pack finish failure is retained and aborts the sink");
    }
    {
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        sink.MismatchedReceipt = true;
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            {},
        };
        expect(parser.consume(response), "mismatched receipt reaches finish");
        const auto result = parser.finish();
        expect(
            !result.has_value() && result.error().PackFailure.has_value()
                && *result.error().PackFailure
                    == application::git_pack_stream_failure::invalid_state
                && sink.Aborted,
            "mismatched expected and observed pack receipt is rejected");
    }
    {
        std::stop_source cancellation;
        cancellation.request_stop();
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        application::detail::git_protocol_v2_fetch_parser parser{
            sink,
            fetch_limits(),
            core::git_hash_algorithm::sha1,
            cancellation.get_token(),
        };
        expect(
            !parser.consume(response) && parser.error()->PackFailure.has_value()
                && *parser.error()->PackFailure
                    == application::git_pack_stream_failure::cancelled
                && sink.Aborted,
            "stop cancellation is propagated by the pack sink and aborts");
    }
}

auto test_end_to_end_acquisition_and_error_mapping() -> void
{
    const auto request_context =
        context(core::git_hash_algorithm::sha256);
    scripted_transport transport{request_context};
    transport.ChunkSize = 1U;

    recording_pack_sink sink{core::git_hash_algorithm::sha256};
    const auto acquired = application::acquire_git_protocol_v2_pack(
        transport,
        request_context,
        {},
        sink,
        fetch_limits(),
        {});
    expect(
        acquired.has_value()
            && acquired->AdvertisedRefs.Base
                == request_context.Revision.Base
            && acquired->AdvertisedRefs.Head
                == request_context.Revision.Head
            && text(sink.Received) == "PACKdata"
            && text(transport.CapturedLsRefsBody).find(
                   "object-format=sha256\n")
                != std::string::npos
            && transport.CapturedMaximumResponseBytes
                == fetch_limits().MaximumWireBytes
            && text(transport.CapturedFetchBody).find(
                   "object-format=sha256\n")
                != std::string::npos && sink.AbortCalls == 0U,
        "one public acquisition validates refs before streaming SHA-256");
    expect(
        transport.Calls
            == std::vector<std::string_view>{
                "discover",
                "ls-refs",
                "fetch",
            },
        "the public operation cannot skip discovery or exact refs");

    {
        scripted_transport failed{request_context};
        failed.DiscoveryFailure =
            application::git_smart_transport_failure::network;
        recording_pack_sink failed_sink{
            core::git_hash_algorithm::sha256};
        const auto result = application::acquire_git_protocol_v2_pack(
            failed,
            request_context,
            {},
            failed_sink,
            fetch_limits(),
            {});
        const auto* error = result.has_value()
            ? nullptr
            : std::get_if<application::git_smart_transport_error>(
                  &result.error());
        expect(
            error != nullptr
                && error->Failure
                    == application::git_smart_transport_failure::network
                && failed_sink.AbortCalls == 1U,
            "discovery transport failure aborts once and retains taxonomy");
    }
    {
        scripted_transport rejected{request_context};
        std::string bad_channel(1U, static_cast<char>(4));
        bad_channel += "data";
        rejected.FetchBody = bytes(
            packet("packfile\n") + packet(bad_channel) + "0000");
        recording_pack_sink rejected_sink{
            core::git_hash_algorithm::sha256};
        const auto result = application::acquire_git_protocol_v2_pack(
            rejected,
            request_context,
            {},
            rejected_sink,
            fetch_limits(),
            {});
        const auto* error = result.has_value()
            ? nullptr
            : std::get_if<application::git_protocol_v2_error>(
                  &result.error());
        expect(
            error != nullptr
                && error->Failure
                    == application::git_protocol_v2_failure::
                        malformed_packet
                && rejected_sink.AbortCalls == 1U,
            "consumer rejection maps to protocol error and aborts once");
    }
    {
        scripted_transport timed_out{request_context};
        timed_out.FetchFailure =
            application::git_smart_transport_failure::timeout;
        recording_pack_sink timed_out_sink{
            core::git_hash_algorithm::sha256};
        const auto result = application::acquire_git_protocol_v2_pack(
            timed_out,
            request_context,
            {},
            timed_out_sink,
            fetch_limits(),
            {});
        const auto* error = result.has_value()
            ? nullptr
            : std::get_if<application::git_smart_transport_error>(
                  &result.error());
        expect(
            error != nullptr
                && error->Failure
                    == application::git_smart_transport_failure::timeout
                && timed_out_sink.AbortCalls == 1U,
            "fetch transport failure remains distinct and aborts once");
    }
    {
        scripted_transport bound{request_context};
        auto stale_context = request_context;
        stale_context.ApplicationSession =
            core::application_session_id{99U};
        recording_pack_sink stale_sink{
            core::git_hash_algorithm::sha256};
        const auto result = application::acquire_git_protocol_v2_pack(
            bound,
            stale_context,
            {},
            stale_sink,
            fetch_limits(),
            {});
        const auto* error = result.has_value()
            ? nullptr
            : std::get_if<application::git_smart_transport_error>(
                  &result.error());
        expect(
            error != nullptr
                && error->Failure
                    == application::git_smart_transport_failure::
                        invalid_request
                && stale_sink.AbortCalls == 1U,
            "a lease rejects a different otherwise-valid request context");
    }
    {
        scripted_transport revoked{request_context};
        revoked.revoke();
        recording_pack_sink revoked_sink{
            core::git_hash_algorithm::sha256};
        const auto result = application::acquire_git_protocol_v2_pack(
            revoked,
            request_context,
            {},
            revoked_sink,
            fetch_limits(),
            {});
        expect(
            !result.has_value() && revoked.revoked()
                && std::holds_alternative<
                    application::git_smart_transport_error>(
                    result.error())
                && revoked_sink.AbortCalls == 1U,
            "a revoked unique lease rejects later work");
    }
}

auto test_public_acquisition_rejects_bypass_inputs() -> void
{
    const auto request_context = context();
    {
        scripted_transport transport{request_context};
        auto invalid_context = request_context;
        invalid_context.ApplicationSession = {};
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        const auto result = application::acquire_git_protocol_v2_pack(
            transport,
            invalid_context,
            {},
            sink,
            fetch_limits(),
            {});
        expect(
            !result.has_value() && transport.Calls.empty()
                && sink.AbortCalls == 1U,
            "invalid context aborts once before a transport round");
    }
    {
        scripted_transport transport{request_context};
        transport.DiscoveryBody = bytes(
            packet("version 2\n") + packet("ls-refs\n")
            + packet("object-format=sha1\n") + "0000");
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        const auto result = application::acquire_git_protocol_v2_pack(
            transport,
            request_context,
            {},
            sink,
            fetch_limits(),
            {});
        expect(
            !result.has_value()
                && transport.Calls
                    == std::vector<std::string_view>{"discover"}
                && sink.AbortCalls == 1U,
            "invalid capabilities cannot reach ls-refs or fetch");
    }
    {
        scripted_transport transport{request_context};
        const auto base = std::string{
            core::canonical_hex(request_context.Revision.Base).view()};
        transport.LsRefsBody = bytes(
            packet(base + " refs/heads/main\n") + "0000");
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        const auto result = application::acquire_git_protocol_v2_pack(
            transport,
            request_context,
            {},
            sink,
            fetch_limits(),
            {});
        expect(
            !result.has_value()
                && transport.Calls
                    == std::vector<std::string_view>{
                        "discover",
                        "ls-refs",
                    }
                && sink.AbortCalls == 1U,
            "incomplete exact refs abort once and cannot reach fetch");
    }
    {
        const auto assert_rejected_before_transport =
            [&](const std::span<const application::git_protocol_v2_have> haves,
                const std::string_view message)
        {
            scripted_transport transport{request_context};
            recording_pack_sink sink{core::git_hash_algorithm::sha1};
            const auto result = application::acquire_git_protocol_v2_pack(
                transport,
                request_context,
                haves,
                sink,
                fetch_limits(),
                {});
            expect(
                !result.has_value() && transport.Calls.empty()
                    && sink.AbortCalls == 1U,
                message);
        };

        application::git_protocol_v2_have candidate{
            .Cache = request_context.Authority.Cache,
            .Commit = oid('3', core::git_hash_algorithm::sha1),
            .CatalogVisible = true,
            .ObjectVerified = true,
        };
        candidate.Cache = opaque<core::repository_cache_scope_id>(99U);
        assert_rejected_before_transport(
            std::span<const application::git_protocol_v2_have>{&candidate, 1U},
            "cross-cache have aborts before any transport round");

        candidate.Cache = request_context.Authority.Cache;
        candidate.CatalogVisible = false;
        assert_rejected_before_transport(
            std::span<const application::git_protocol_v2_have>{&candidate, 1U},
            "catalog-invisible have aborts before any transport round");

        candidate.CatalogVisible = true;
        candidate.ObjectVerified = false;
        assert_rejected_before_transport(
            std::span<const application::git_protocol_v2_have>{&candidate, 1U},
            "unverified have aborts before any transport round");

        candidate.ObjectVerified = true;
        candidate.Commit = oid('3', core::git_hash_algorithm::sha256);
        assert_rejected_before_transport(
            std::span<const application::git_protocol_v2_have>{&candidate, 1U},
            "mixed-format have aborts before any transport round");

        const std::vector excessive_haves(
            application::maximum_git_protocol_v2_haves + 1U,
            application::git_protocol_v2_have{
                .Cache = request_context.Authority.Cache,
                .Commit = oid('3', core::git_hash_algorithm::sha1),
                .CatalogVisible = true,
                .ObjectVerified = true,
            });
        assert_rejected_before_transport(
            excessive_haves,
            "excessive have count aborts before any transport round");
    }
    {
        auto zero_wire = fetch_limits();
        zero_wire.MaximumWireBytes = 0U;
        auto excessive_wire = fetch_limits();
        excessive_wire.MaximumWireBytes =
            application::maximum_git_smart_fetch_response_bytes + 1U;
        auto zero_pack = fetch_limits();
        zero_pack.MaximumPackBytes = 0U;
        auto excessive_pack = fetch_limits();
        excessive_pack.MaximumPackBytes =
            application::maximum_git_protocol_v2_pack_bytes + 1U;
        auto excessive_progress = fetch_limits();
        excessive_progress.MaximumProgressBytes =
            application::maximum_git_protocol_v2_progress_bytes + 1U;
        auto zero_packets = fetch_limits();
        zero_packets.MaximumPackets = 0U;
        auto excessive_packets = fetch_limits();
        excessive_packets.MaximumPackets =
            application::maximum_git_protocol_v2_fetch_packets + 1U;

        for(const auto invalid_limits : {
                zero_wire,
                excessive_wire,
                zero_pack,
                excessive_pack,
                excessive_progress,
                zero_packets,
                excessive_packets,
            })
        {
            scripted_transport transport{request_context};
            recording_pack_sink sink{core::git_hash_algorithm::sha1};
            const auto result = application::acquire_git_protocol_v2_pack(
                transport,
                request_context,
                {},
                sink,
                invalid_limits,
                {});
            expect(
                !result.has_value() && transport.Calls.empty()
                    && sink.AbortCalls == 1U,
                "invalid or overflowing limits abort before transport");
        }
    }
    {
        scripted_transport transport{request_context};
        std::stop_source cancellation;
        cancellation.request_stop();
        recording_pack_sink sink{core::git_hash_algorithm::sha1};
        const auto result = application::acquire_git_protocol_v2_pack(
            transport,
            request_context,
            {},
            sink,
            fetch_limits(),
            cancellation.get_token());
        expect(
            !result.has_value() && transport.Calls.empty()
                && sink.AbortCalls == 1U,
            "pre-cancelled acquisition aborts once without network work");
    }
}
} // namespace

int main()
{
    test_base_ref_and_context_validation();
    test_advertisement_and_requests();
    test_ls_refs_strict_identity();
    test_incremental_fetch_parser();
    test_fetch_parser_rejections();
    test_fetch_limits_and_framing();
    test_pack_sink_failures_and_cancellation();
    test_end_to_end_acquisition_and_error_mapping();
    test_public_acquisition_rejects_bypass_inputs();
    return EXIT_SUCCESS;
}
