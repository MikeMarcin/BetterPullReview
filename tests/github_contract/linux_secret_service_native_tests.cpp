#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "linux_secret_service.hpp"
#include "linux_secret_service_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <spawn.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr;
using namespace bpr::adapters::github;
using namespace bpr::tests::linux_secret_service;

inline constexpr std::size_t native_canary_a_bytes = 73U;
inline constexpr std::size_t native_canary_b_bytes =
    application::maximum_github_credential_bytes;
inline constexpr std::size_t native_leak_probe_bytes = 32U;
inline constexpr std::uint64_t native_session =
    0x4250522400000001ULL;
inline constexpr std::uint64_t native_generation =
    0x4250522400000002ULL;
inline constexpr std::uint64_t maximum_audited_file_bytes =
    128U * 1024U * 1024U;
inline constexpr int child_ready_descriptor = 100;
inline constexpr int child_control_descriptor = 101;

using leak_probe = std::span<const std::byte>;
using leak_probes = std::span<const leak_probe>;

static_assert(native_leak_probe_bytes <= native_canary_a_bytes);
static_assert(native_leak_probe_bytes <= native_canary_b_bytes);

class secure_bytes final
{
public:
    explicit secure_bytes(const std::size_t size)
        : Bytes(size)
    {
    }

    ~secure_bytes() noexcept
    {
        reset();
    }

    secure_bytes(const secure_bytes&) = delete;
    auto operator=(const secure_bytes&) -> secure_bytes& = delete;
    secure_bytes(secure_bytes&&) noexcept = default;
    auto operator=(secure_bytes&& other) noexcept -> secure_bytes&
    {
        if(this != &other)
        {
            reset();
            Bytes = std::move(other.Bytes);
        }
        return *this;
    }

    [[nodiscard]] auto mutable_bytes() noexcept -> std::span<std::byte>
    {
        return Bytes;
    }

    [[nodiscard]] auto bytes() const noexcept
        -> std::span<const std::byte>
    {
        return Bytes;
    }

private:
    void reset() noexcept
    {
        erase(Bytes);
        Bytes.clear();
    }

    std::vector<std::byte> Bytes;
};

class file_descriptor final
{
public:
    file_descriptor() = default;
    explicit file_descriptor(const int descriptor) noexcept
        : Descriptor{descriptor}
    {
    }

    ~file_descriptor() noexcept
    {
        reset();
    }

    file_descriptor(const file_descriptor&) = delete;
    auto operator=(const file_descriptor&) -> file_descriptor& = delete;

    file_descriptor(file_descriptor&& other) noexcept
        : Descriptor{std::exchange(other.Descriptor, -1)}
    {
    }

    auto operator=(file_descriptor&& other) noexcept -> file_descriptor&
    {
        if(this != &other)
        {
            reset();
            Descriptor = std::exchange(other.Descriptor, -1);
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> int
    {
        return Descriptor;
    }

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return Descriptor >= 0;
    }

    void reset(const int descriptor = -1) noexcept
    {
        if(valid())
        {
            static_cast<void>(::close(Descriptor));
        }
        Descriptor = descriptor;
    }

private:
    int Descriptor = -1;
};

class audit_directory final
{
public:
    audit_directory() = default;
    ~audit_directory() noexcept
    {
        reset();
    }

    audit_directory(const audit_directory&) = delete;
    auto operator=(const audit_directory&) -> audit_directory& = delete;

    audit_directory(audit_directory&& other) noexcept
        : Path{std::move(other.Path)}
    {
        other.Path.clear();
    }

    auto operator=(audit_directory&& other) noexcept
        -> audit_directory&
    {
        if(this != &other)
        {
            reset();
            Path = std::move(other.Path);
            other.Path.clear();
        }
        return *this;
    }

    [[nodiscard]] static auto create()
        -> std::optional<audit_directory>
    {
        std::error_code temporary_error;
        const auto temporary_root =
            std::filesystem::temp_directory_path(temporary_error);
        if(temporary_error)
        {
            return std::nullopt;
        }

        auto pattern =
            (temporary_root
             / "betterpullreview-secret-service-audit-XXXXXX")
                .string();
        auto mutable_pattern =
            std::vector<char>{pattern.begin(), pattern.end()};
        mutable_pattern.push_back('\0');
        const auto* created = ::mkdtemp(mutable_pattern.data());
        if(created == nullptr)
        {
            return std::nullopt;
        }

        audit_directory directory;
        directory.Path = created;
        for(const auto* child : {
                "home",
                "tmp",
                "config",
                "data",
                "cache",
            })
        {
            std::error_code error;
            if(
                !std::filesystem::create_directory(
                    directory.Path / child,
                    error)
                || error)
            {
                return std::nullopt;
            }
        }
        return directory;
    }

    [[nodiscard]] auto path() const noexcept
        -> const std::filesystem::path&
    {
        return Path;
    }

    [[nodiscard]] auto remove() noexcept -> bool
    {
        if(Path.empty())
        {
            return true;
        }

        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(Path, error));
        if(error)
        {
            return false;
        }
        const auto remains = std::filesystem::exists(Path, error);
        if(error || remains)
        {
            return false;
        }
        Path.clear();
        return true;
    }

    void abandon() noexcept
    {
        // A child that is not confirmed stopped may still use this exact
        // directory. Preserve it for deliberate cleanup instead of removing
        // storage beneath a live process.
        Path.clear();
    }

private:
    void reset() noexcept
    {
        if(!Path.empty())
        {
            std::error_code ignored;
            std::filesystem::remove_all(Path, ignored);
            Path.clear();
        }
    }

    std::filesystem::path Path;
};

template<typename Completion>
struct terminal_recorder
{
    std::size_t Calls = 0U;
    std::optional<Completion> Terminal;

    void record(Completion completion)
    {
        ++Calls;
        Terminal.emplace(std::move(completion));
    }
};

using store_recorder =
    terminal_recorder<application::github_credential_store_completion>;
using retrieve_recorder =
    terminal_recorder<application::github_credential_retrieve_completion>;
using delete_recorder =
    terminal_recorder<application::github_credential_delete_completion>;

template<typename Terminal, typename Completion>
[[nodiscard]] auto terminal_as(const std::optional<Completion>& completion)
    -> const Terminal*
{
    return completion.has_value()
        ? std::get_if<Terminal>(&*completion)
        : nullptr;
}

[[nodiscard]] auto native_authority()
    -> application::github_credential_authority_key
{
    return authority(
        0x4250522400000011ULL,
        0x4250522400000012ULL,
        0x4250522400000013ULL,
        0x4250522400000014ULL);
}

[[nodiscard]] auto native_context(const std::uint64_t request)
    -> application::github_credential_operation_context
{
    return context(
        native_authority(),
        request,
        native_session,
        native_generation);
}

void write_fixed_diagnostic(
    const std::string_view category) noexcept
{
    auto remaining = category;
    while(!remaining.empty())
    {
        const auto written =
            ::write(STDERR_FILENO, remaining.data(), remaining.size());
        if(written < 0 && errno == EINTR)
        {
            continue;
        }
        if(written <= 0)
        {
            return;
        }
        remaining.remove_prefix(static_cast<std::size_t>(written));
    }
}

[[nodiscard]] auto run_store(
    application::github_credential_vault_port& vault,
    application::manual_executor& lane,
    const std::uint64_t request,
    const std::span<const std::byte> bytes) -> bool
{
    auto material = application::github_credential_material::create(bytes);
    if(!material.has_value())
    {
        return false;
    }

    const auto operation = native_context(request);
    store_recorder recorder;
    vault.store(
        application::github_credential_enrollment{
            .Context = operation,
            .Credential = std::move(*material),
        },
        [&recorder](
            application::github_credential_store_completion terminal) {
            recorder.record(std::move(terminal));
        });
    if(lane.pending() != 1U)
    {
        vault.cancel(operation);
        lane.drain();
        return false;
    }
    lane.run_next();

    const auto* success = terminal_as<
        application::github_credential_store_success>(recorder.Terminal);
    return recorder.Calls == 1U && success != nullptr
        && success->Context == operation
        && success->Disposition
            == application::github_credential_store_disposition::stored;
}

[[nodiscard]] auto run_retrieve(
    application::github_credential_vault_port& vault,
    application::manual_executor& lane,
    const std::uint64_t request,
    const std::span<const std::byte> expected) -> bool
{
    const auto operation = native_context(request);
    retrieve_recorder recorder;
    vault.retrieve(
        operation,
        [&recorder](
            application::github_credential_retrieve_completion terminal) {
            recorder.record(std::move(terminal));
        });
    if(lane.pending() != 1U)
    {
        vault.cancel(operation);
        lane.drain();
        return false;
    }
    lane.run_next();

    const auto* success = terminal_as<
        application::github_credential_retrieve_success>(
        recorder.Terminal);
    if(recorder.Calls != 1U)
    {
        write_fixed_diagnostic(
            "linux-secret-service-retrieve:terminal-count\n");
        return false;
    }
    if(success == nullptr)
    {
        const auto* error = terminal_as<
            application::github_credential_retrieve_error>(
            recorder.Terminal);
        if(error == nullptr)
        {
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:unexpected-terminal\n");
            return false;
        }
        switch(error->Failure)
        {
        case application::github_credential_vault_failure::none:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:failure-none\n");
            break;
        case application::github_credential_vault_failure::locked:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:locked\n");
            break;
        case application::github_credential_vault_failure::unavailable:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:unavailable\n");
            break;
        case application::github_credential_vault_failure::access_denied:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:access-denied\n");
            break;
        case application::github_credential_vault_failure::corrupt:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:corrupt\n");
            break;
        case application::github_credential_vault_failure::
            allocation_failure:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:allocation-failure\n");
            break;
        case application::github_credential_vault_failure::invalid_request:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:invalid-request\n");
            break;
        case application::github_credential_vault_failure::operation_failed:
            write_fixed_diagnostic(
                "linux-secret-service-retrieve:operation-failed\n");
            break;
        }
        return false;
    }
    if(success->Context != operation)
    {
        write_fixed_diagnostic(
            "linux-secret-service-retrieve:context\n");
        return false;
    }
    if(!same_bytes(success->Credential.bytes(), expected))
    {
        write_fixed_diagnostic(
            "linux-secret-service-retrieve:bytes\n");
        return false;
    }
    return true;
}

[[nodiscard]] auto run_retrieve_absent(
    application::github_credential_vault_port& vault,
    application::manual_executor& lane,
    const std::uint64_t request) -> bool
{
    const auto operation = native_context(request);
    retrieve_recorder recorder;
    vault.retrieve(
        operation,
        [&recorder](
            application::github_credential_retrieve_completion terminal) {
            recorder.record(std::move(terminal));
        });
    if(lane.pending() != 1U)
    {
        vault.cancel(operation);
        lane.drain();
        return false;
    }
    lane.run_next();

    const auto* absent = terminal_as<
        application::github_credential_retrieve_absent>(
        recorder.Terminal);
    return recorder.Calls == 1U && absent != nullptr
        && absent->Context == operation;
}

[[nodiscard]] auto run_delete(
    application::github_credential_vault_port& vault,
    application::manual_executor& lane,
    const std::uint64_t request,
    const std::optional<
        application::github_credential_delete_disposition>
        expected) -> bool
{
    const auto operation = native_context(request);
    delete_recorder recorder;
    vault.delete_credential(
        operation,
        [&recorder](
            application::github_credential_delete_completion terminal) {
            recorder.record(std::move(terminal));
        });
    if(lane.pending() != 1U)
    {
        vault.cancel(operation);
        lane.drain();
        return false;
    }
    lane.run_next();

    const auto* success = terminal_as<
        application::github_credential_delete_success>(
        recorder.Terminal);
    return recorder.Calls == 1U && success != nullptr
        && success->Context == operation
        && (!expected.has_value()
            || success->Disposition == *expected);
}

[[nodiscard]] auto make_native_vault(
    application::manual_executor& lane)
    -> std::unique_ptr<application::github_credential_vault_port>
{
    auto vault =
        make_linux_secret_service_integration_test_vault(lane);
    return vault.has_value() ? std::move(*vault) : nullptr;
}

[[nodiscard]] auto ensure_native_item_absent(
    const std::uint64_t request_base) -> bool
{
    application::manual_executor lane;
    auto vault = make_native_vault(lane);
    if(vault == nullptr)
    {
        return false;
    }

    const auto deletion =
        run_delete(*vault, lane, request_base, std::nullopt);
    const auto absence =
        run_retrieve_absent(*vault, lane, request_base + 1U);
    return deletion && absence;
}

void record_lifecycle_result(
    const bool result,
    const std::string_view category,
    bool& aggregate) noexcept
{
    if(!result)
    {
        write_fixed_diagnostic(category);
    }
    aggregate = result && aggregate;
}

[[nodiscard]] auto run_native_lifecycle(
    const std::span<const std::byte> canary_a,
    const std::span<const std::byte> canary_b) -> bool
{
    application::manual_executor lane;
    auto vault = make_native_vault(lane);
    if(vault == nullptr)
    {
        return false;
    }

    auto ok = true;
    record_lifecycle_result(
        run_delete(*vault, lane, 100U, std::nullopt),
        "linux-secret-service-lifecycle:initial-delete\n",
        ok);
    record_lifecycle_result(
        run_retrieve_absent(*vault, lane, 101U),
        "linux-secret-service-lifecycle:initial-absence\n",
        ok);
    record_lifecycle_result(
        run_store(*vault, lane, 102U, canary_a),
        "linux-secret-service-lifecycle:first-store\n",
        ok);
    record_lifecycle_result(
        run_retrieve(*vault, lane, 103U, canary_a),
        "linux-secret-service-lifecycle:first-retrieve\n",
        ok);
    record_lifecycle_result(
        run_store(*vault, lane, 104U, canary_b),
        "linux-secret-service-lifecycle:replacement-store\n",
        ok);
    record_lifecycle_result(
        run_retrieve(*vault, lane, 105U, canary_b),
        "linux-secret-service-lifecycle:replacement-retrieve\n",
        ok);
    record_lifecycle_result(
        run_delete(
            *vault,
            lane,
            106U,
            application::github_credential_delete_disposition::deleted),
        "linux-secret-service-lifecycle:delete\n",
        ok);
    record_lifecycle_result(
        run_retrieve_absent(*vault, lane, 107U),
        "linux-secret-service-lifecycle:post-delete-absence\n",
        ok);
    record_lifecycle_result(
        run_delete(
            *vault,
            lane,
            108U,
            application::github_credential_delete_disposition::absent),
        "linux-secret-service-lifecycle:repeat-delete\n",
        ok);

    const auto cleanup =
        run_delete(*vault, lane, 109U, std::nullopt);
    const auto post_cleanup_absence =
        run_retrieve_absent(*vault, lane, 110U);
    record_lifecycle_result(
        cleanup,
        "linux-secret-service-lifecycle:cleanup\n",
        ok);
    record_lifecycle_result(
        post_cleanup_absence,
        "linux-secret-service-lifecycle:cleanup-absence\n",
        ok);
    return ok && cleanup && post_cleanup_absence;
}

[[nodiscard]] auto read_exact(
    const int descriptor,
    const std::span<std::byte> output) -> bool
{
    auto offset = std::size_t{0U};
    while(offset < output.size())
    {
        const auto count = ::read(
            descriptor,
            output.data() + offset,
            output.size() - offset);
        if(count > 0)
        {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if(count < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] auto write_exact(
    const int descriptor,
    const std::span<const std::byte> input) -> bool
{
    auto offset = std::size_t{0U};
    while(offset < input.size())
    {
        const auto count = ::write(
            descriptor,
            input.data() + offset,
            input.size() - offset);
        if(count > 0)
        {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if(count < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] auto read_frame(const int descriptor)
    -> std::optional<secure_bytes>
{
    std::array<std::byte, 4U> header{};
    if(!read_exact(descriptor, header))
    {
        return std::nullopt;
    }
    const auto length =
        (static_cast<std::uint32_t>(
             std::to_integer<std::uint8_t>(header[0]))
         << 24U)
        | (static_cast<std::uint32_t>(
               std::to_integer<std::uint8_t>(header[1]))
           << 16U)
        | (static_cast<std::uint32_t>(
               std::to_integer<std::uint8_t>(header[2]))
           << 8U)
        | static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(header[3]));
    if(
        length == 0U
        || length > application::maximum_github_credential_bytes)
    {
        return std::nullopt;
    }

    secure_bytes bytes{length};
    if(!read_exact(descriptor, bytes.mutable_bytes()))
    {
        return std::nullopt;
    }
    return bytes;
}

[[nodiscard]] auto write_frame(
    const int descriptor,
    const std::span<const std::byte> bytes) -> bool
{
    if(
        bytes.empty()
        || bytes.size() > application::maximum_github_credential_bytes)
    {
        return false;
    }
    const auto length = static_cast<std::uint32_t>(bytes.size());
    const std::array header{
        static_cast<std::byte>((length >> 24U) & 0xFFU),
        static_cast<std::byte>((length >> 16U) & 0xFFU),
        static_cast<std::byte>((length >> 8U) & 0xFFU),
        static_cast<std::byte>(length & 0xFFU),
    };
    return write_exact(descriptor, header)
        && write_exact(descriptor, bytes);
}

[[nodiscard]] auto printable(
    const std::span<const std::byte> bytes) -> bool
{
    return std::ranges::all_of(bytes, [](const std::byte byte) {
        const auto value = std::to_integer<unsigned int>(byte);
        return value >= 0x21U && value <= 0x7EU;
    });
}

[[nodiscard]] auto fill_random(
    const std::span<std::byte> bytes) -> bool
{
    auto offset = std::size_t{0U};
    while(offset < bytes.size())
    {
        const auto count = ::getrandom(
            bytes.data() + offset,
            bytes.size() - offset,
            0U);
        if(count > 0)
        {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if(count < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
    return true;
}

[[nodiscard]] auto random_printable_bytes(const std::size_t size)
    -> std::optional<secure_bytes>
{
    secure_bytes bytes{size};
    if(!fill_random(bytes.mutable_bytes()))
    {
        return std::nullopt;
    }
    for(auto& byte : bytes.mutable_bytes())
    {
        const auto random = std::to_integer<unsigned int>(byte);
        byte = static_cast<std::byte>(0x21U + (random % 94U));
    }
    return bytes;
}

[[nodiscard]] auto opaque_bytes(
    const std::size_t size,
    const std::size_t salt) -> secure_bytes
{
    secure_bytes bytes{size};
    auto output = bytes.mutable_bytes();
    for(std::size_t index = 0U; index < output.size(); ++index)
    {
        output[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(
                ((index * 197U) + salt) & 0xFFU));
    }

    constexpr std::array required_prefix{
        std::byte{0x00U},
        std::byte{0xFFU},
        std::byte{0xC0U},
        std::byte{0xAFU},
        std::byte{0x80U},
        std::byte{0xFEU},
    };
    std::ranges::copy(required_prefix, output.begin());
    return bytes;
}

[[nodiscard]] auto run_expected_store_failure(
    const application::github_credential_vault_failure expected,
    const std::uint64_t request) -> bool
{
    auto canary = random_printable_bytes(native_canary_a_bytes);
    if(!canary.has_value())
    {
        return false;
    }

    application::manual_executor lane;
    auto factory =
        make_linux_secret_service_integration_test_vault(lane);
    if(!factory.has_value())
    {
        return factory.error() == expected;
    }
    auto vault = std::move(*factory);
    const auto operation = native_context(request);
    auto material = application::github_credential_material::create(
        canary->bytes());
    if(!material.has_value())
    {
        return false;
    }

    store_recorder recorder;
    vault->store(
        application::github_credential_enrollment{
            .Context = operation,
            .Credential = std::move(*material),
        },
        [&recorder](
            application::github_credential_store_completion terminal) {
            recorder.record(std::move(terminal));
        });
    if(lane.pending() != 1U)
    {
        vault->cancel(operation);
        lane.drain();
        return false;
    }
    lane.run_next();

    const auto* error = terminal_as<
        application::github_credential_store_error>(recorder.Terminal);
    if(
        recorder.Calls == 1U && error != nullptr
        && error->Context == operation && error->Failure == expected)
    {
        return true;
    }

    // An unexpected success must not leave the exact integration item.
    static_cast<void>(
        run_delete(*vault, lane, request + 1U, std::nullopt));
    return false;
}

[[nodiscard]] auto run_bus_redirect_rejection() -> bool
{
    constexpr std::string_view hostile_bus_name =
        "org.betterpullreview.Hostile.Secret";
    const auto* configured = std::getenv("SECRET_SERVICE_BUS_NAME");
    if(
        configured == nullptr
        || std::string_view{configured} != hostile_bus_name)
    {
        return false;
    }

    return run_expected_store_failure(
        application::github_credential_vault_failure::unavailable,
        5'000U);
}

[[nodiscard]] auto write_category(
    const std::string_view category) -> bool
{
    return write_exact(
        STDOUT_FILENO,
        std::as_bytes(std::span{category}));
}

void report_audit_failure(
    const bool condition,
    const std::string_view category) noexcept
{
    if(!condition)
    {
        static_cast<void>(write_exact(
            STDERR_FILENO,
            std::as_bytes(std::span{category})));
    }
}

[[nodiscard]] auto native_child() -> int
{
    auto canary_a = read_frame(STDIN_FILENO);
    auto canary_b = read_frame(STDIN_FILENO);
    std::byte extra{};
    const auto trailing = ::read(STDIN_FILENO, &extra, 1U);
    const auto input_valid =
        canary_a.has_value() && canary_b.has_value()
        && canary_a->bytes().size() == native_canary_a_bytes
        && canary_b->bytes().size() == native_canary_b_bytes
        && !same_bytes(canary_a->bytes(), canary_b->bytes())
        && printable(canary_a->bytes())
        && printable(canary_b->bytes()) && trailing == 0;

    constexpr std::array ready{std::byte{0x52U}};
    std::array<std::byte, 1U> control{};
    const auto synchronized =
        input_valid
        && write_exact(child_ready_descriptor, ready)
        && read_exact(child_control_descriptor, control)
        && control[0] == std::byte{0x47U};
    auto opaque_a = opaque_bytes(native_canary_a_bytes, 17U);
    auto opaque_b = opaque_bytes(native_canary_b_bytes, 41U);
    const auto opaque_shape =
        !printable(opaque_a.bytes()) && !printable(opaque_b.bytes())
        && opaque_a.bytes().front() == std::byte{0x00U}
        && opaque_a.bytes()[1U] == std::byte{0xFFU}
        && opaque_a.bytes()[2U] == std::byte{0xC0U}
        && opaque_a.bytes()[3U] == std::byte{0xAFU}
        && opaque_a.bytes()[4U] == std::byte{0x80U}
        && opaque_b.bytes().front() == std::byte{0x00U}
        && opaque_b.bytes()[1U] == std::byte{0xFFU};
    const auto passed =
        synchronized
        && run_native_lifecycle(
            canary_a->bytes(),
            canary_b->bytes())
        && opaque_shape
        && run_native_lifecycle(opaque_a.bytes(), opaque_b.bytes());
    static_cast<void>(write_category(
        passed ? "linux-secret-service-native:pass\n"
               : "linux-secret-service-native:fail\n"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] auto string_contains(
    const std::string_view text,
    const leak_probes probes) -> bool
{
    return std::ranges::any_of(probes, [text](const leak_probe probe) {
        if(probe.empty() || text.size() < probe.size())
        {
            return false;
        }
        return std::search(
                   text.begin(),
                   text.end(),
                   probe.begin(),
                   probe.end(),
                   [](const char character, const std::byte byte) {
                       return static_cast<unsigned char>(character)
                           == std::to_integer<unsigned char>(byte);
                   })
            != text.end();
    });
}

[[nodiscard]] auto read_regular_file(
    const std::filesystem::path& path)
    -> std::optional<secure_bytes>
{
    file_descriptor input{
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
    if(!input.valid())
    {
        return std::nullopt;
    }

    struct stat information
    {
    };
    if(
        ::fstat(input.get(), &information) != 0
        || !S_ISREG(information.st_mode) || information.st_size < 0
        || static_cast<std::uint64_t>(information.st_size)
            > maximum_audited_file_bytes)
    {
        return std::nullopt;
    }

    secure_bytes bytes{
        static_cast<std::size_t>(information.st_size)};
    if(!read_exact(input.get(), bytes.mutable_bytes()))
    {
        return std::nullopt;
    }
    return bytes;
}

[[nodiscard]] auto read_bounded_stream(
    const std::filesystem::path& path,
    const std::size_t maximum_bytes)
    -> std::optional<secure_bytes>
{
    file_descriptor input{
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if(!input.valid())
    {
        return std::nullopt;
    }

    std::vector<std::byte> accumulated;
    try
    {
        accumulated.reserve(std::min(maximum_bytes, std::size_t{4'096U}));
        std::array<std::byte, 4'096U> block{};
        while(true)
        {
            const auto count =
                ::read(input.get(), block.data(), block.size());
            if(count > 0)
            {
                const auto received = static_cast<std::size_t>(count);
                if(received > maximum_bytes - accumulated.size())
                {
                    erase(block);
                    erase(accumulated);
                    return std::nullopt;
                }
                accumulated.insert(
                    accumulated.end(),
                    block.begin(),
                    block.begin()
                        + static_cast<std::ptrdiff_t>(received));
                erase(block);
                continue;
            }
            erase(block);
            if(count == 0)
            {
                break;
            }
            if(errno == EINTR)
            {
                continue;
            }
            erase(accumulated);
            return std::nullopt;
        }
    }
    catch(...)
    {
        erase(accumulated);
        return std::nullopt;
    }

    secure_bytes bytes{accumulated.size()};
    std::ranges::copy(accumulated, bytes.mutable_bytes().begin());
    erase(accumulated);
    return bytes;
}

[[nodiscard]] auto probes_absent(
    const std::span<const std::byte> bytes,
    const leak_probes probes) -> bool
{
    return std::ranges::none_of(probes, [bytes](const leak_probe probe) {
        return std::ranges::search(bytes, probe).begin() != bytes.end();
    });
}

[[nodiscard]] auto probes_absent_from_file(
    const std::filesystem::path& path,
    const leak_probes probes) -> bool
{
    auto bytes = read_regular_file(path);
    return bytes.has_value()
        && probes_absent(bytes->bytes(), probes);
}

[[nodiscard]] auto probes_absent_from_stream(
    const std::filesystem::path& path,
    const leak_probes probes) -> bool
{
    auto bytes = read_bounded_stream(path, 1024U * 1024U);
    return bytes.has_value()
        && probes_absent(bytes->bytes(), probes);
}

[[nodiscard]] auto exact_file_text(
    const std::filesystem::path& path,
    const std::string_view expected) -> bool
{
    auto bytes = read_regular_file(path);
    return bytes.has_value()
        && bytes->bytes().size() == expected.size()
        && std::ranges::equal(
            bytes->bytes(),
            std::as_bytes(std::span{expected}));
}

[[nodiscard]] auto file_contains_text(
    const std::filesystem::path& path,
    const std::string_view expected) -> bool
{
    auto bytes = read_regular_file(path);
    if(!bytes.has_value())
    {
        return false;
    }
    const auto expected_bytes =
        std::as_bytes(std::span{expected});
    return std::ranges::search(
               bytes->bytes(), expected_bytes)
               .begin()
        != bytes->bytes().end();
}

[[nodiscard]] auto probes_absent_from_tree(
    const std::filesystem::path& root,
    const leak_probes probes) -> bool
{
    std::error_code error;
    auto iterator = std::filesystem::recursive_directory_iterator{
        root,
        std::filesystem::directory_options::none,
        error};
    const auto end = std::filesystem::recursive_directory_iterator{};
    if(error)
    {
        return false;
    }
    while(iterator != end)
    {
        const auto entry = *iterator;
        iterator.increment(error);
        if(error)
        {
            return false;
        }
        if(entry.is_regular_file(error))
        {
            if(
                error
                || !probes_absent_from_file(entry.path(), probes))
            {
                return false;
            }
        }
        else if(error)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto environment_value(
    const std::string_view name) -> std::optional<std::string>
{
    const auto* value = std::getenv(std::string{name}.c_str());
    if(value == nullptr || *value == '\0')
    {
        return std::nullopt;
    }
    return std::string{value};
}

[[nodiscard]] auto make_child_environment(
    const std::filesystem::path& root)
    -> std::optional<std::vector<std::string>>
{
    const auto session_bus =
        environment_value("DBUS_SESSION_BUS_ADDRESS");
    if(!session_bus.has_value())
    {
        return std::nullopt;
    }

    std::vector<std::string> environment{
        "DBUS_SESSION_BUS_ADDRESS=" + *session_bus,
        "HOME=" + (root / "home").string(),
        "LANG=C",
        "PATH=/usr/bin:/bin",
        "TMPDIR=" + (root / "tmp").string(),
        "XDG_CACHE_HOME=" + (root / "cache").string(),
        "XDG_CONFIG_HOME=" + (root / "config").string(),
        "XDG_DATA_HOME=" + (root / "data").string(),
    };
    if(const auto runtime = environment_value("XDG_RUNTIME_DIR");
       runtime.has_value())
    {
        environment.push_back("XDG_RUNTIME_DIR=" + *runtime);
    }
    std::ranges::sort(environment);
    return environment;
}

[[nodiscard]] auto make_pipe()
    -> std::optional<std::array<file_descriptor, 2U>>
{
    std::array<int, 2U> raw{-1, -1};
    if(::pipe2(raw.data(), O_CLOEXEC) != 0)
    {
        return std::nullopt;
    }
    return std::array{
        file_descriptor{raw[0]},
        file_descriptor{raw[1]},
    };
}

[[nodiscard]] auto add_dup(
    posix_spawn_file_actions_t& actions,
    const int source,
    const int destination) -> bool
{
    return posix_spawn_file_actions_adddup2(
               &actions,
               source,
               destination)
        == 0;
}

[[nodiscard]] auto add_close(
    posix_spawn_file_actions_t& actions,
    const int descriptor) -> bool
{
    return posix_spawn_file_actions_addclose(&actions, descriptor) == 0;
}

[[nodiscard]] auto wait_until_readable(
    const int descriptor,
    const std::chrono::milliseconds timeout) -> bool
{
    pollfd requested{
        .fd = descriptor,
        .events = POLLIN,
        .revents = 0,
    };
    while(true)
    {
        const auto result =
            ::poll(&requested, 1U, static_cast<int>(timeout.count()));
        if(result > 0)
        {
            return (requested.revents & POLLIN) != 0;
        }
        if(result < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
}

[[nodiscard]] auto wait_for_child(
    const pid_t child,
    const std::chrono::steady_clock::time_point deadline,
    int& status) -> bool
{
    while(std::chrono::steady_clock::now() < deadline)
    {
        const auto result = ::waitpid(child, &status, WNOHANG);
        if(result == child)
        {
            return true;
        }
        if(result < 0 && errno == EINTR)
        {
            continue;
        }
        if(result < 0)
        {
            return false;
        }
        timespec delay{
            .tv_sec = 0,
            .tv_nsec = 10'000'000L,
        };
        while(::nanosleep(&delay, &delay) != 0 && errno == EINTR)
        {
        }
    }
    return false;
}

[[nodiscard]] auto stop_and_reap_child(
    const pid_t child,
    int& status) -> bool
{
    if(::kill(child, SIGKILL) != 0 && errno != ESRCH)
    {
        return false;
    }
    while(true)
    {
        const auto result = ::waitpid(child, &status, 0);
        if(result == child)
        {
            return true;
        }
        if(result < 0 && errno == EINTR)
        {
            continue;
        }
        return result < 0 && errno == ECHILD;
    }
}

[[nodiscard]] auto audit_native_child(
    const int parent_argument_count,
    char* const parent_arguments[]) -> bool
{
    auto canary_a = random_printable_bytes(native_canary_a_bytes);
    auto canary_b = random_printable_bytes(native_canary_b_bytes);
    if(!canary_a.has_value() || !canary_b.has_value())
    {
        return false;
    }
    while(same_bytes(canary_a->bytes(), canary_b->bytes()))
    {
        canary_b = random_printable_bytes(native_canary_b_bytes);
        if(!canary_b.has_value())
        {
            return false;
        }
    }
    const std::array probes{
        canary_a->bytes().first(native_leak_probe_bytes),
        canary_a->bytes().last(native_leak_probe_bytes),
        canary_b->bytes().first(native_leak_probe_bytes),
        canary_b->bytes().last(native_leak_probe_bytes),
    };

    auto directory = audit_directory::create();
    std::error_code executable_error;
    const auto executable =
        std::filesystem::canonical("/proc/self/exe", executable_error);
    if(!directory.has_value() || executable_error)
    {
        return false;
    }

    const auto stdout_path = directory->path() / "stdout.audit";
    const auto stderr_path = directory->path() / "stderr.audit";
    file_descriptor stdout_file{::open(
        stdout_path.c_str(),
        O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
        S_IRUSR | S_IWUSR)};
    file_descriptor stderr_file{::open(
        stderr_path.c_str(),
        O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
        S_IRUSR | S_IWUSR)};
    auto input = make_pipe();
    auto ready = make_pipe();
    auto control = make_pipe();
    auto environment = make_child_environment(directory->path());
    if(
        !stdout_file.valid() || !stderr_file.valid()
        || !input.has_value() || !ready.has_value()
        || !control.has_value() || !environment.has_value())
    {
        return false;
    }

    const std::array owned_descriptors{
        stdout_file.get(),
        stderr_file.get(),
        (*input)[0].get(),
        (*input)[1].get(),
        (*ready)[0].get(),
        (*ready)[1].get(),
        (*control)[0].get(),
        (*control)[1].get(),
    };
    if(std::ranges::any_of(owned_descriptors, [](const int descriptor) {
           return descriptor <= STDERR_FILENO
               || descriptor == child_ready_descriptor
               || descriptor == child_control_descriptor;
       }))
    {
        return false;
    }

    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions) != 0)
    {
        return false;
    }
    auto actions_valid =
        add_dup(actions, stdout_file.get(), STDOUT_FILENO)
        && add_dup(actions, stderr_file.get(), STDERR_FILENO)
        && add_dup(actions, (*input)[0].get(), STDIN_FILENO)
        && add_dup(
            actions,
            (*ready)[1].get(),
            child_ready_descriptor)
        && add_dup(
            actions,
            (*control)[0].get(),
            child_control_descriptor);
    for(const auto descriptor : owned_descriptors)
    {
        actions_valid =
            add_close(actions, descriptor) && actions_valid;
    }
    actions_valid =
        posix_spawn_file_actions_addchdir_np(
            &actions,
            directory->path().c_str())
            == 0
        && actions_valid;

    std::vector<char*> environment_pointers;
    environment_pointers.reserve(environment->size() + 1U);
    for(auto& value : *environment)
    {
        environment_pointers.push_back(value.data());
    }
    environment_pointers.push_back(nullptr);

    auto executable_text = executable.string();
    auto child_mode = std::string{"--native-child"};
    std::array<char*, 3U> child_arguments{
        executable_text.data(),
        child_mode.data(),
        nullptr,
    };

    auto audit_ok = actions_valid;
    for(auto index = 0; index < parent_argument_count; ++index)
    {
        audit_ok =
            !string_contains(parent_arguments[index], probes) && audit_ok;
    }
    for(const auto* argument : child_arguments)
    {
        if(argument != nullptr)
        {
            audit_ok =
                !string_contains(argument, probes) && audit_ok;
        }
    }
    for(const auto& value : *environment)
    {
        audit_ok = !string_contains(value, probes) && audit_ok;
    }

    pid_t child = -1;
    const auto spawn_status =
        actions_valid
        ? posix_spawn(
              &child,
              executable_text.c_str(),
              &actions,
              nullptr,
              child_arguments.data(),
              environment_pointers.data())
        : EINVAL;
    static_cast<void>(posix_spawn_file_actions_destroy(&actions));

    (*input)[0].reset();
    (*ready)[1].reset();
    (*control)[0].reset();
    stdout_file.reset();
    stderr_file.reset();
    if(spawn_status != 0)
    {
        return false;
    }

    const auto wrote =
        write_frame((*input)[1].get(), canary_a->bytes())
        && write_frame((*input)[1].get(), canary_b->bytes());
    (*input)[1].reset();

    std::array<std::byte, 1U> ready_signal{};
    const auto child_ready =
        wait_until_readable(
            (*ready)[0].get(),
            std::chrono::seconds{5})
        && read_exact((*ready)[0].get(), ready_signal)
        && ready_signal[0] == std::byte{0x52U};
    (*ready)[0].reset();

    const auto child_process_root =
        std::filesystem::path{"/proc"}
        / std::to_string(static_cast<long long>(child));
    const auto actual_inputs_clean =
        child_ready
        && probes_absent_from_stream(
            child_process_root / "cmdline",
            probes)
        && probes_absent_from_stream(
            child_process_root / "environ",
            probes);
    constexpr std::array proceed{std::byte{0x47U}};
    const auto released =
        write_exact((*control)[1].get(), proceed);
    (*control)[1].reset();

    int child_status = 0;
    const auto waited =
        wait_for_child(
            child,
            std::chrono::steady_clock::now()
                + std::chrono::seconds{15},
            child_status);
    const auto process_stopped =
        waited || stop_and_reap_child(child, child_status);
    if(!process_stopped)
    {
        directory->abandon();
        return false;
    }

    const auto parent_cleanup =
        ensure_native_item_absent(3'000U);
    const auto child_exited =
        waited && WIFEXITED(child_status);
    const auto child_succeeded =
        child_exited && WEXITSTATUS(child_status) == EXIT_SUCCESS;
    const auto stdout_is_exact = exact_file_text(
        stdout_path,
        "linux-secret-service-native:pass\n");
    const auto stderr_is_empty =
        exact_file_text(stderr_path, "");
    report_audit_failure(
        actions_valid,
        "linux-secret-service-audit:spawn-actions\n");
    report_audit_failure(
        wrote,
        "linux-secret-service-audit:frame-write\n");
    report_audit_failure(
        child_ready,
        "linux-secret-service-audit:child-ready\n");
    report_audit_failure(
        actual_inputs_clean,
        "linux-secret-service-audit:process-input-scan\n");
    report_audit_failure(
        released,
        "linux-secret-service-audit:child-release\n");
    report_audit_failure(
        waited,
        "linux-secret-service-audit:child-timeout\n");
    report_audit_failure(
        child_exited,
        "linux-secret-service-audit:child-exit\n");
    report_audit_failure(
        child_succeeded,
        "linux-secret-service-audit:child-lifecycle\n");
    report_audit_failure(
        stdout_is_exact,
        "linux-secret-service-audit:stdout\n");
    report_audit_failure(
        stderr_is_empty,
        "linux-secret-service-audit:stderr\n");
    for(const auto category : {
            std::string_view{
                "linux-secret-service-lifecycle:initial-delete\n"},
            std::string_view{
                "linux-secret-service-lifecycle:initial-absence\n"},
            std::string_view{
                "linux-secret-service-lifecycle:first-store\n"},
            std::string_view{
                "linux-secret-service-lifecycle:first-retrieve\n"},
            std::string_view{
                "linux-secret-service-lifecycle:replacement-store\n"},
            std::string_view{
                "linux-secret-service-lifecycle:replacement-retrieve\n"},
            std::string_view{
                "linux-secret-service-lifecycle:delete\n"},
            std::string_view{
                "linux-secret-service-lifecycle:post-delete-absence\n"},
            std::string_view{
                "linux-secret-service-lifecycle:repeat-delete\n"},
            std::string_view{
                "linux-secret-service-lifecycle:cleanup\n"},
            std::string_view{
                "linux-secret-service-lifecycle:cleanup-absence\n"},
            std::string_view{
                "linux-secret-service-retrieve:terminal-count\n"},
            std::string_view{
                "linux-secret-service-retrieve:unexpected-terminal\n"},
            std::string_view{
                "linux-secret-service-retrieve:failure-none\n"},
            std::string_view{
                "linux-secret-service-retrieve:locked\n"},
            std::string_view{
                "linux-secret-service-retrieve:unavailable\n"},
            std::string_view{
                "linux-secret-service-retrieve:access-denied\n"},
            std::string_view{
                "linux-secret-service-retrieve:corrupt\n"},
            std::string_view{
                "linux-secret-service-retrieve:allocation-failure\n"},
            std::string_view{
                "linux-secret-service-retrieve:invalid-request\n"},
            std::string_view{
                "linux-secret-service-retrieve:operation-failed\n"},
            std::string_view{
                "linux-secret-service-retrieve:context\n"},
            std::string_view{
                "linux-secret-service-retrieve:bytes\n"},
        })
    {
        report_audit_failure(
            !file_contains_text(stderr_path, category),
            category);
    }
    report_audit_failure(
        parent_cleanup,
        "linux-secret-service-audit:parent-cleanup\n");
    audit_ok =
        wrote && child_ready && actual_inputs_clean && released
        && child_succeeded && stdout_is_exact && stderr_is_empty
        && parent_cleanup && audit_ok;

    std::error_code source_error;
    const auto source =
        std::filesystem::canonical(__FILE__, source_error);
    const auto source_resolved = !source_error;
    const auto executable_is_clean =
        probes_absent_from_file(executable, probes);
    const auto source_is_clean =
        source_resolved && probes_absent_from_file(source, probes);
    const auto scratch_is_clean =
        probes_absent_from_tree(directory->path(), probes);
    report_audit_failure(
        source_resolved,
        "linux-secret-service-audit:source-resolution\n");
    report_audit_failure(
        executable_is_clean,
        "linux-secret-service-audit:executable-scan\n");
    report_audit_failure(
        source_is_clean,
        "linux-secret-service-audit:source-scan\n");
    report_audit_failure(
        scratch_is_clean,
        "linux-secret-service-audit:scratch-scan\n");
    audit_ok =
        source_resolved && executable_is_clean && source_is_clean
        && scratch_is_clean && audit_ok;
    const auto directory_removed = directory->remove();
    report_audit_failure(
        directory_removed,
        "linux-secret-service-audit:scratch-cleanup\n");
    return directory_removed && audit_ok;
}

[[nodiscard]] auto fixture_mode(const bool setup) -> int
{
    const auto request_base = setup ? 1'000U : 2'000U;
    const auto passed = ensure_native_item_absent(request_base);
    const auto success_category =
        setup ? "linux-secret-service-setup:pass\n"
              : "linux-secret-service-cleanup:pass\n";
    const auto failure_category =
        setup ? "linux-secret-service-setup:fail\n"
              : "linux-secret-service-cleanup:fail\n";
    static_cast<void>(
        write_category(passed ? success_category : failure_category));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] auto expected_failure_mode(
    const application::github_credential_vault_failure expected,
    const std::string_view pass_category,
    const std::string_view fail_category) -> int
{
    const auto passed = run_expected_store_failure(expected, 4'000U);
    static_cast<void>(
        write_category(passed ? pass_category : fail_category));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
}

auto main(const int argument_count, char* arguments[]) -> int
{
    static_cast<void>(std::signal(SIGPIPE, SIG_IGN));
    if(
        argument_count == 2
        && std::string_view{arguments[1]} == "--native-child")
    {
        return native_child();
    }
    if(
        argument_count == 2
        && std::string_view{arguments[1]} == "--fixture-setup")
    {
        return fixture_mode(true);
    }
    if(
        argument_count == 2
        && std::string_view{arguments[1]} == "--fixture-cleanup")
    {
        return fixture_mode(false);
    }
    if(
        argument_count == 2
        && std::string_view{arguments[1]} == "--expect-unavailable")
    {
        const auto no_explicit_session_bus =
            std::getenv("DBUS_SESSION_BUS_ADDRESS") == nullptr;
        if(!no_explicit_session_bus)
        {
            static_cast<void>(write_category(
                "linux-secret-service-unavailable:fail\n"));
            return EXIT_FAILURE;
        }
        return expected_failure_mode(
            application::github_credential_vault_failure::unavailable,
            "linux-secret-service-unavailable:pass\n",
            "linux-secret-service-unavailable:fail\n");
    }
    if(
        argument_count == 2
        && std::string_view{arguments[1]} == "--expect-locked")
    {
        return expected_failure_mode(
            application::github_credential_vault_failure::locked,
            "linux-secret-service-locked:pass\n",
            "linux-secret-service-locked:fail\n");
    }
    if(
        argument_count == 2
        && std::string_view{arguments[1]}
            == "--expect-bus-redirect-rejected")
    {
        const auto passed = run_bus_redirect_rejection();
        static_cast<void>(write_category(
            passed
                ? "linux-secret-service-bus-redirect-rejected:pass\n"
                : "linux-secret-service-bus-redirect-rejected:fail\n"));
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if(argument_count != 1)
    {
        static_cast<void>(
            write_category("linux-secret-service-audit:fail\n"));
        return EXIT_FAILURE;
    }

    const auto passed =
        audit_native_child(argument_count, arguments);
    static_cast<void>(write_category(
        passed ? "linux-secret-service-audit:pass\n"
               : "linux-secret-service-audit:fail\n"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
