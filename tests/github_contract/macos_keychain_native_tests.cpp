#include "macos_keychain.hpp"
#include "macos_keychain_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <Security/SecRandom.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fcntl.h>
#include <optional>
#include <spawn.h>
#include <span>
#include <string>
#include <string_view>
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
using namespace bpr::tests::macos_keychain;

inline constexpr std::size_t native_canary_bytes = 73U;
inline constexpr std::uint64_t native_session = 0x4250522200000001ULL;
inline constexpr std::uint64_t native_generation = 0x4250522200000002ULL;
inline constexpr std::size_t maximum_audited_file_bytes =
    128U * 1024U * 1024U;

class secure_bytes final
{
public:
    explicit secure_bytes(const std::size_t size)
        : Bytes(size)
    {
    }

    ~secure_bytes() noexcept
    {
        erase(Bytes);
    }

    secure_bytes(const secure_bytes&) = delete;
    auto operator=(const secure_bytes&) -> secure_bytes& = delete;
    secure_bytes(secure_bytes&&) noexcept = default;
    auto operator=(secure_bytes&& other) noexcept -> secure_bytes&
    {
        if(this != &other)
        {
            erase(Bytes);
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

    [[nodiscard]] auto release() noexcept -> int
    {
        return std::exchange(Descriptor, -1);
    }

    void reset(const int descriptor = -1) noexcept
    {
        if(Descriptor >= 0)
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

    [[nodiscard]] static auto create()
        -> std::optional<audit_directory>
    {
        std::array<char, 45U> pattern{};
        constexpr std::string_view prefix =
            "/tmp/betterpullreview-keychain-audit-XXXXXX";
        std::ranges::copy(prefix, pattern.begin());
        auto* created = ::mkdtemp(pattern.data());
        if(created == nullptr)
        {
            return std::nullopt;
        }

        audit_directory directory;
        directory.Path = created;
        std::error_code error;
        for(const auto& child : {
                "home",
                "tmp",
                "config",
                "data",
                "cache",
            })
        {
            std::filesystem::create_directory(
                directory.Path / child,
                error);
            if(error)
            {
                return std::nullopt;
            }
        }
        return directory;
    }

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

    [[nodiscard]] auto path() const noexcept
        -> const std::filesystem::path&
    {
        return Path;
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
        0x4250522200000011ULL,
        0x4250522200000012ULL,
        0x4250522200000013ULL,
        0x4250522200000014ULL);
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
        return false;
    }
    lane.run_next();

    const auto* success = terminal_as<
        application::github_credential_retrieve_success>(
        recorder.Terminal);
    return recorder.Calls == 1U && success != nullptr
        && success->Context == operation
        && same_bytes(success->Credential.bytes(), expected);
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
    auto vault = make_macos_keychain_integration_test_vault(lane);
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
    ok = run_delete(*vault, lane, 100U, std::nullopt) && ok;
    ok = run_retrieve_absent(*vault, lane, 101U) && ok;
    ok = run_store(*vault, lane, 102U, canary_a) && ok;
    ok = run_retrieve(*vault, lane, 103U, canary_a) && ok;
    ok = run_store(*vault, lane, 104U, canary_b) && ok;
    ok = run_retrieve(*vault, lane, 105U, canary_b) && ok;
    ok = run_delete(
             *vault,
             lane,
             106U,
             application::github_credential_delete_disposition::deleted)
        && ok;
    ok = run_retrieve_absent(*vault, lane, 107U) && ok;
    ok = run_delete(
             *vault,
             lane,
             108U,
             application::github_credential_delete_disposition::absent)
        && ok;

    const auto unconditional_cleanup =
        run_delete(*vault, lane, 109U, std::nullopt);
    const auto post_cleanup_absence =
        run_retrieve_absent(*vault, lane, 110U);
    return ok && unconditional_cleanup && post_cleanup_absence;
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

[[nodiscard]] auto random_printable_bytes()
    -> std::optional<secure_bytes>
{
    secure_bytes bytes{native_canary_bytes};
    if(
        SecRandomCopyBytes(
            kSecRandomDefault,
            bytes.mutable_bytes().size(),
            bytes.mutable_bytes().data())
        != errSecSuccess)
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

[[nodiscard]] auto write_category(
    const std::string_view category) -> bool
{
    return write_exact(
        STDOUT_FILENO,
        std::as_bytes(std::span{category}));
}

[[nodiscard]] auto native_child() -> int
{
    auto canary_a = read_frame(STDIN_FILENO);
    auto canary_b = read_frame(STDIN_FILENO);
    std::byte extra{};
    const auto trailing = ::read(STDIN_FILENO, &extra, 1U);
    const auto input_valid =
        canary_a.has_value() && canary_b.has_value()
        && !same_bytes(canary_a->bytes(), canary_b->bytes())
        && printable(canary_a->bytes())
        && printable(canary_b->bytes()) && trailing == 0;
    const auto passed =
        input_valid
        && run_native_lifecycle(
            canary_a->bytes(),
            canary_b->bytes());
    static_cast<void>(write_category(
        passed ? "macos-keychain-native:pass\n"
               : "macos-keychain-native:fail\n"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] auto string_contains(
    const std::string_view text,
    const std::span<const std::byte> bytes) -> bool
{
    if(bytes.empty() || text.size() < bytes.size())
    {
        return false;
    }
    return std::search(
               text.begin(),
               text.end(),
               bytes.begin(),
               bytes.end(),
               [](const char character, const std::byte byte) {
                   return static_cast<unsigned char>(character)
                       == std::to_integer<unsigned char>(byte);
               })
        != text.end();
}

[[nodiscard]] auto read_file(
    const std::filesystem::path& path)
    -> std::optional<secure_bytes>
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if(error || size > maximum_audited_file_bytes)
    {
        return std::nullopt;
    }

    file_descriptor input{
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if(input.get() < 0)
    {
        return std::nullopt;
    }
    secure_bytes bytes{static_cast<std::size_t>(size)};
    if(!read_exact(input.get(), bytes.mutable_bytes()))
    {
        return std::nullopt;
    }
    return bytes;
}

[[nodiscard]] auto file_contains(
    const std::filesystem::path& path,
    const std::span<const std::byte> needle)
    -> std::optional<bool>
{
    auto bytes = read_file(path);
    if(!bytes.has_value())
    {
        return std::nullopt;
    }
    return std::ranges::search(bytes->bytes(), needle).begin()
        != bytes->bytes().end();
}

[[nodiscard]] auto exact_file_text(
    const std::filesystem::path& path,
    const std::string_view expected) -> bool
{
    auto bytes = read_file(path);
    return bytes.has_value()
        && bytes->bytes().size() == expected.size()
        && std::ranges::equal(
            bytes->bytes(),
            std::as_bytes(std::span{expected}));
}

[[nodiscard]] auto canaries_absent_from_file(
    const std::filesystem::path& path,
    const std::span<const std::byte> canary_a,
    const std::span<const std::byte> canary_b) -> bool
{
    const auto contains_a = file_contains(path, canary_a);
    const auto contains_b = file_contains(path, canary_b);
    return contains_a.has_value() && contains_b.has_value()
        && !*contains_a && !*contains_b;
}

[[nodiscard]] auto canaries_absent_from_tree(
    const std::filesystem::path& root,
    const std::span<const std::byte> canary_a,
    const std::span<const std::byte> canary_b) -> bool
{
    std::error_code error;
    auto iterator = std::filesystem::recursive_directory_iterator{
        root,
        std::filesystem::directory_options::skip_permission_denied,
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
                || !canaries_absent_from_file(
                    entry.path(),
                    canary_a,
                    canary_b))
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

[[nodiscard]] auto make_child_environment(
    const std::filesystem::path& root) -> std::vector<std::string>
{
    const auto* home = std::getenv("HOME");
    if(home == nullptr || *home == '\0')
    {
        return {};
    }
    return {
        "HOME=" + std::string{home},
        "TMPDIR=" + (root / "tmp").string(),
        "XDG_CONFIG_HOME=" + (root / "config").string(),
        "XDG_DATA_HOME=" + (root / "data").string(),
        "XDG_CACHE_HOME=" + (root / "cache").string(),
        "PATH=/usr/bin:/bin",
        "LANG=C",
    };
}

[[nodiscard]] auto audit_native_child(
    const int parent_argument_count,
    char* const parent_arguments[]) -> bool
{
    auto canary_a = random_printable_bytes();
    auto canary_b = random_printable_bytes();
    if(!canary_a.has_value() || !canary_b.has_value())
    {
        return false;
    }
    while(same_bytes(canary_a->bytes(), canary_b->bytes()))
    {
        canary_b = random_printable_bytes();
        if(!canary_b.has_value())
        {
            return false;
        }
    }

    auto directory = audit_directory::create();
    if(!directory.has_value())
    {
        return false;
    }

    std::error_code path_error;
    const auto executable =
        std::filesystem::canonical(parent_arguments[0], path_error);
    if(path_error)
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
    std::array<int, 2U> input_pipe{-1, -1};
    if(
        stdout_file.get() < 0 || stderr_file.get() < 0
        || ::pipe(input_pipe.data()) != 0)
    {
        if(input_pipe[0] >= 0)
        {
            static_cast<void>(::close(input_pipe[0]));
        }
        if(input_pipe[1] >= 0)
        {
            static_cast<void>(::close(input_pipe[1]));
        }
        return false;
    }
    file_descriptor child_input{input_pipe[0]};
    file_descriptor parent_input{input_pipe[1]};
    if(::fcntl(parent_input.get(), F_SETNOSIGPIPE, 1) != 0)
    {
        return false;
    }

    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions) != 0)
    {
        return false;
    }
    auto actions_valid = true;
    actions_valid =
        posix_spawn_file_actions_adddup2(
            &actions,
            child_input.get(),
            STDIN_FILENO)
            == 0
        && actions_valid;
    actions_valid =
        posix_spawn_file_actions_adddup2(
            &actions,
            stdout_file.get(),
            STDOUT_FILENO)
            == 0
        && actions_valid;
    actions_valid =
        posix_spawn_file_actions_adddup2(
            &actions,
            stderr_file.get(),
            STDERR_FILENO)
            == 0
        && actions_valid;
    actions_valid =
        posix_spawn_file_actions_addclose(
            &actions,
            parent_input.get())
            == 0
        && actions_valid;
    actions_valid =
        posix_spawn_file_actions_addchdir_np(
            &actions,
            directory->path().c_str())
            == 0
        && actions_valid;

    auto environment =
        make_child_environment(directory->path());
    if(environment.empty())
    {
        static_cast<void>(
            posix_spawn_file_actions_destroy(&actions));
        return false;
    }
    std::vector<char*> environment_pointers;
    environment_pointers.reserve(environment.size() + 1U);
    for(auto& value : environment)
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
        const auto argument =
            std::string_view{parent_arguments[index]};
        audit_ok =
            !string_contains(argument, canary_a->bytes())
            && !string_contains(argument, canary_b->bytes())
            && audit_ok;
    }
    for(const auto* argument : child_arguments)
    {
        if(argument != nullptr)
        {
            const auto value = std::string_view{argument};
            audit_ok =
                !string_contains(value, canary_a->bytes())
                && !string_contains(value, canary_b->bytes())
                && audit_ok;
        }
    }
    for(const auto& value : environment)
    {
        audit_ok =
            !string_contains(value, canary_a->bytes())
            && !string_contains(value, canary_b->bytes())
            && audit_ok;
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
    child_input.reset();
    stdout_file.reset();
    stderr_file.reset();
    if(spawn_status != 0)
    {
        return false;
    }

    const auto wrote =
        write_frame(parent_input.get(), canary_a->bytes())
        && write_frame(parent_input.get(), canary_b->bytes());
    parent_input.reset();

    int child_status = 0;
    auto waited = false;
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds{15};
    while(std::chrono::steady_clock::now() < deadline)
    {
        const auto result =
            ::waitpid(child, &child_status, WNOHANG);
        if(result == child)
        {
            waited = true;
            break;
        }
        if(result < 0 && errno == EINTR)
        {
            continue;
        }
        if(result < 0)
        {
            break;
        }
        static_cast<void>(::usleep(10'000U));
    }
    if(!waited)
    {
        static_cast<void>(::kill(child, SIGKILL));
        while(::waitpid(child, &child_status, 0) < 0 && errno == EINTR)
        {
        }
    }
    const auto parent_cleanup =
        ensure_native_item_absent(3'000U);

    audit_ok =
        waited && wrote && WIFEXITED(child_status)
        && WEXITSTATUS(child_status) == EXIT_SUCCESS && audit_ok;
    audit_ok =
        exact_file_text(
            stdout_path,
            "macos-keychain-native:pass\n")
        && exact_file_text(stderr_path, "") && audit_ok;
    audit_ok =
        canaries_absent_from_file(
            executable,
            canary_a->bytes(),
            canary_b->bytes())
        && canaries_absent_from_file(
            __FILE__,
            canary_a->bytes(),
            canary_b->bytes())
        && canaries_absent_from_tree(
            directory->path(),
            canary_a->bytes(),
            canary_b->bytes())
        && parent_cleanup && audit_ok;
    return audit_ok;
}

[[nodiscard]] auto fixture_mode(
    const bool setup) -> int
{
    const auto request_base =
        setup ? 1'000U : 2'000U;
    const auto passed = ensure_native_item_absent(request_base);
    static_cast<void>(write_category(
        setup
            ? (passed ? "macos-keychain-setup:pass\n"
                      : "macos-keychain-setup:fail\n")
            : (passed ? "macos-keychain-cleanup:pass\n"
                      : "macos-keychain-cleanup:fail\n")));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
}

auto main(const int argument_count, char* arguments[]) -> int
{
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
    if(argument_count != 1)
    {
        static_cast<void>(
            write_category("macos-keychain-audit:fail\n"));
        return EXIT_FAILURE;
    }

    const auto passed =
        audit_native_child(argument_count, arguments);
    static_cast<void>(write_category(
        passed ? "macos-keychain-audit:pass\n"
               : "macos-keychain-audit:fail\n"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
