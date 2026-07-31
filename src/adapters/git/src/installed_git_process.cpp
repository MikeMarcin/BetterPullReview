#include "bpr/adapters/git/installed_git_process.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(Q_OS_UNIX)
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace bpr::adapters::git
{
namespace
{
using application::git_ancestry;
using application::git_commit_id;
using application::git_hash_algorithm;
using application::git_pack_id;
using application::git_process_failure;
using application::git_process_result;
using application::git_tree_id;
using clock = std::chrono::steady_clock;

inline constexpr auto wait_slice = std::chrono::milliseconds{10};
inline constexpr auto process_timeout = std::chrono::seconds{30};
inline constexpr auto pack_process_timeout = std::chrono::minutes{5};
inline constexpr auto reap_timeout = std::chrono::seconds{2};
inline constexpr auto start_timeout = std::chrono::seconds{5};
inline constexpr std::size_t standard_output_limit = 1024U;
inline constexpr std::size_t standard_error_limit = 16U * 1024U;
inline constexpr std::size_t input_chunk_bytes = 16U * 1024U;
inline constexpr std::uint64_t maximum_pack_index_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr qint64 maximum_pending_input_bytes = 64 * 1024;
inline constexpr std::string_view transaction_success =
    "start: ok\nprepare: ok\ncommit: ok\n";

struct process_success
{
    int ExitCode = 0;
    std::string StandardOutput;
};

using process_result =
    std::expected<process_success, git_process_failure>;

struct memory_input
{
    std::string Bytes;
};

struct file_input
{
    std::filesystem::path Path;
    std::uint64_t Bytes = 0U;
};

using process_input = std::variant<memory_input, file_input>;

struct process_request
{
    std::vector<std::string> Arguments;
    process_input Input = memory_input{};
    std::chrono::milliseconds Timeout = process_timeout;
    bool AllowExitOne = false;
};

[[nodiscard]] auto path_text(
    const std::filesystem::path& path) -> std::string
{
#if defined(Q_OS_WIN)
    const auto native = path.u8string();
    return {
        reinterpret_cast<const char*>(native.data()),
        native.size()};
#else
    return path.string();
#endif
}

[[nodiscard]] auto decode_text(
    const std::string_view value,
    QString& decoded) -> bool
{
    if(
        value.size()
        > static_cast<std::size_t>(
            std::numeric_limits<qsizetype>::max()))
    {
        return false;
    }
    decoded = QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size()));
    const auto encoded = decoded.toUtf8();
    return static_cast<std::size_t>(encoded.size()) == value.size()
        && std::equal(
            value.begin(),
            value.end(),
            encoded.constData());
}

[[nodiscard]] auto canonical_existing(
    const std::filesystem::path& path)
    -> std::optional<std::filesystem::path>
{
    std::error_code error;
    if(path.empty() || !path.is_absolute())
    {
        return std::nullopt;
    }
    const auto canonical = std::filesystem::canonical(path, error);
    if(error || canonical != path.lexically_normal())
    {
        return std::nullopt;
    }
    return canonical;
}

[[nodiscard]] auto resolve_canonical_existing(
    const std::filesystem::path& path)
    -> std::optional<std::filesystem::path>
{
    std::error_code error;
    if(path.empty() || !path.is_absolute())
    {
        return std::nullopt;
    }
    const auto canonical = std::filesystem::canonical(path, error);
    return error
        ? std::nullopt
        : std::optional<std::filesystem::path>{canonical};
}

[[nodiscard]] auto private_directory(
    const std::filesystem::path& path) -> bool
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if(
        error || status.type() != std::filesystem::file_type::directory
        || std::filesystem::is_symlink(status)
        || canonical_existing(path) != path)
    {
        return false;
    }
#if defined(Q_OS_UNIX)
    struct stat information
    {
    };
    if(::lstat(path.c_str(), &information) != 0)
    {
        return false;
    }
    return S_ISDIR(information.st_mode)
        && information.st_uid == ::geteuid()
        && (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#else
#if defined(Q_OS_WIN)
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
#else
    return true;
#endif
#endif
}

[[nodiscard]] auto empty_private_directory(
    const std::filesystem::path& path) -> bool
{
    if(!private_directory(path))
    {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_empty(path, error) && !error;
}

[[nodiscard]] auto private_regular_file(
    const std::filesystem::path& path) -> bool
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if(
        error
        || status.type() != std::filesystem::file_type::regular
        || std::filesystem::is_symlink(status)
        || canonical_existing(path) != path)
    {
        return false;
    }
#if defined(Q_OS_UNIX)
    struct stat information
    {
    };
    if(::lstat(path.c_str(), &information) != 0)
    {
        return false;
    }
    return S_ISREG(information.st_mode)
        && information.st_uid == ::geteuid()
        && (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#else
#if defined(Q_OS_WIN)
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
#else
    return true;
#endif
#endif
}

class private_input_file final
{
public:
    private_input_file() = default;
    private_input_file(const private_input_file&) = delete;
    auto operator=(const private_input_file&) -> private_input_file& = delete;

    ~private_input_file()
    {
#if defined(Q_OS_UNIX)
        if(Descriptor >= 0)
        {
            static_cast<void>(::close(Descriptor));
        }
#elif defined(Q_OS_WIN)
        if(Handle != INVALID_HANDLE_VALUE)
        {
            static_cast<void>(CloseHandle(Handle));
        }
#endif
    }

    [[nodiscard]] auto open(
        const std::filesystem::path& path,
        const std::uint64_t expected_bytes) -> bool
    {
        if(
            path.empty() || !path.is_absolute()
            || !private_directory(path.parent_path()))
        {
            return false;
        }
#if defined(Q_OS_UNIX)
        Descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if(Descriptor < 0)
        {
            return false;
        }
        struct stat information
        {
        };
        return ::fstat(Descriptor, &information) == 0
            && S_ISREG(information.st_mode)
            && information.st_uid == ::geteuid()
            && information.st_nlink == 1
            && (information.st_mode & (S_IRWXG | S_IRWXO)) == 0
            && information.st_size >= 0
            && static_cast<std::uint64_t>(information.st_size)
                == expected_bytes;
#elif defined(Q_OS_WIN)
        Handle = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            0U,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
                | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        if(Handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        FILE_ATTRIBUTE_TAG_INFO tag{};
        BY_HANDLE_FILE_INFORMATION information{};
        LARGE_INTEGER size{};
        return GetFileInformationByHandleEx(
                   Handle,
                   FileAttributeTagInfo,
                   &tag,
                   sizeof(tag))
                != 0
            && (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U
            && (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U
            && GetFileInformationByHandle(Handle, &information) != 0
            && information.nNumberOfLinks == 1U
            && GetFileSizeEx(Handle, &size) != 0 && size.QuadPart >= 0
            && static_cast<std::uint64_t>(size.QuadPart)
                == expected_bytes;
#else
        static_cast<void>(expected_bytes);
        return false;
#endif
    }

    [[nodiscard]] auto read(const std::span<char> destination)
        -> std::optional<std::size_t>
    {
#if defined(Q_OS_UNIX)
        for(;;)
        {
            const auto received = ::read(
                Descriptor,
                destination.data(),
                destination.size());
            if(received < 0 && errno == EINTR)
            {
                continue;
            }
            if(received < 0)
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(received);
        }
#elif defined(Q_OS_WIN)
        const auto requested = static_cast<DWORD>(
            std::min<std::size_t>(
                destination.size(),
                std::numeric_limits<DWORD>::max()));
        DWORD received = 0U;
        if(
            ReadFile(
                Handle,
                destination.data(),
                requested,
                &received,
                nullptr)
            == 0)
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(received);
#else
        static_cast<void>(destination);
        return std::nullopt;
#endif
    }

private:
#if defined(Q_OS_UNIX)
    int Descriptor = -1;
#elif defined(Q_OS_WIN)
    HANDLE Handle = INVALID_HANDLE_VALUE;
#endif
};

[[nodiscard]] auto regular_executable(
    const std::filesystem::path& path) -> bool
{
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if(error || !std::filesystem::is_regular_file(status))
    {
        return false;
    }
#if defined(Q_OS_WIN)
    return true;
#else
    const auto permissions = status.permissions();
    return (permissions
            & (std::filesystem::perms::owner_exec
               | std::filesystem::perms::group_exec
               | std::filesystem::perms::others_exec))
        != std::filesystem::perms::none;
#endif
}

auto make_private_directory(
    const std::filesystem::path& path) -> bool
{
    if(path.empty() || !path.is_absolute())
    {
        return false;
    }
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if(
        error
        && error
            != std::make_error_code(
                std::errc::no_such_file_or_directory))
    {
        return false;
    }
    if(
        !error
        && status.type() != std::filesystem::file_type::not_found)
    {
        return private_directory(path);
    }
    error.clear();
    if(
        !private_directory(path.parent_path())
        || !std::filesystem::create_directory(path, error)
        || error)
    {
        return false;
    }
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace,
        error);
    return !error && private_directory(path);
}

auto write_private_file(
    const std::filesystem::path& path,
    const std::span<const std::byte> bytes) -> bool
{
    if(
        path.empty() || !path.is_absolute()
        || !private_directory(path.parent_path()))
    {
        return false;
    }
#if defined(Q_OS_UNIX)
    struct stat before
    {
    };
    const auto existed = ::lstat(path.c_str(), &before) == 0;
    if(
        existed
        && (!S_ISREG(before.st_mode)
            || before.st_uid != ::geteuid()
            || before.st_nlink != 1
            || (before.st_mode & (S_IRWXG | S_IRWXO)) != 0))
    {
        return false;
    }
    if(!existed && errno != ENOENT)
    {
        return false;
    }

    const auto flags = O_WRONLY | O_CLOEXEC | O_NOFOLLOW
        | (existed ? 0 : O_CREAT | O_EXCL);
    const auto descriptor = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if(descriptor < 0)
    {
        return false;
    }
    const auto close_descriptor =
        [descriptor]() noexcept
    {
        return ::close(descriptor) == 0;
    };

    struct stat opened
    {
    };
    auto valid = ::fstat(descriptor, &opened) == 0
        && S_ISREG(opened.st_mode)
        && opened.st_uid == ::geteuid()
        && opened.st_nlink == 1
        && (!existed
            || (opened.st_dev == before.st_dev
                && opened.st_ino == before.st_ino))
        && ::fchmod(descriptor, S_IRUSR | S_IWUSR) == 0
        && ::ftruncate(descriptor, 0) == 0;
    std::size_t offset = 0U;
    while(valid && offset < bytes.size())
    {
        const auto written = ::write(
            descriptor,
            bytes.data() + offset,
            bytes.size() - offset);
        if(written < 0 && errno == EINTR)
        {
            continue;
        }
        if(written <= 0)
        {
            valid = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    valid = valid && ::fsync(descriptor) == 0;
    const auto closed = close_descriptor();
    return valid && closed && private_regular_file(path);
#elif defined(Q_OS_WIN)
    const auto attributes = GetFileAttributesW(path.c_str());
    const auto existed = attributes != INVALID_FILE_ATTRIBUTES;
    if(
        existed
        && ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
            || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U))
    {
        return false;
    }
    const auto handle = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0U,
        nullptr,
        existed ? OPEN_EXISTING : CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    FILE_ATTRIBUTE_TAG_INFO tag{};
    BY_HANDLE_FILE_INFORMATION information{};
    LARGE_INTEGER beginning{};
    auto valid =
        GetFileInformationByHandleEx(
            handle,
            FileAttributeTagInfo,
            &tag,
            sizeof(tag))
            != 0
        && (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U
        && GetFileInformationByHandle(handle, &information) != 0
        && information.nNumberOfLinks == 1U
        && SetFilePointerEx(handle, beginning, nullptr, FILE_BEGIN) != 0
        && SetEndOfFile(handle) != 0;
    std::size_t offset = 0U;
    while(valid && offset < bytes.size())
    {
        const auto remaining = std::min<std::size_t>(
            bytes.size() - offset,
            std::numeric_limits<DWORD>::max());
        DWORD written = 0U;
        valid = WriteFile(
                    handle,
                    bytes.data() + offset,
                    static_cast<DWORD>(remaining),
                    &written,
                    nullptr)
                != 0
            && written != 0U;
        offset += written;
    }
    valid = valid && FlushFileBuffers(handle) != 0;
    const auto closed = CloseHandle(handle) != 0;
    return valid && closed && private_regular_file(path);
#else
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if(
        error
        || (status.type() != std::filesystem::file_type::not_found
            && (status.type()
                    != std::filesystem::file_type::regular
                || std::filesystem::is_symlink(status))))
    {
        return false;
    }
    try
    {
        std::ofstream output{
            path,
            std::ios::binary | std::ios::trunc};
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        output.close();
        if(!output)
        {
            return false;
        }
    }
    catch(...)
    {
        return false;
    }
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        error);
    return !error && private_regular_file(path);
#endif
}

auto write_private_file(
    const std::filesystem::path& path,
    const std::string_view bytes) -> bool
{
    return write_private_file(
        path,
        std::as_bytes(std::span{bytes}));
}

[[nodiscard]] auto read_exact_file(
    const std::filesystem::path& path,
    const std::string_view expected) -> bool
{
    try
    {
        if(!private_regular_file(path))
        {
            return false;
        }
        std::ifstream input{path, std::ios::binary};
        if(!input)
        {
            return false;
        }
        std::string value{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        return !input.bad() && value == expected;
    }
    catch(...)
    {
        return false;
    }
}

[[nodiscard]] auto equal_private_files(
    const std::filesystem::path& left,
    const std::filesystem::path& right,
    const std::uint64_t expected_bytes) -> bool
{
    try
    {
        if(
            !private_regular_file(left)
            || !private_regular_file(right))
        {
            return false;
        }
        std::error_code error;
        const auto left_size =
            std::filesystem::file_size(left, error);
        if(error || left_size != expected_bytes)
        {
            return false;
        }
        const auto right_size =
            std::filesystem::file_size(right, error);
        if(error || right_size != expected_bytes)
        {
            return false;
        }

        std::ifstream left_input{left, std::ios::binary};
        std::ifstream right_input{right, std::ios::binary};
        if(!left_input || !right_input)
        {
            return false;
        }
        std::array<char, 64U * 1024U> left_buffer{};
        std::array<char, 64U * 1024U> right_buffer{};
        auto remaining = expected_bytes;
        while(remaining != 0U)
        {
            const auto requested = static_cast<std::streamsize>(
                std::min<std::uint64_t>(
                    left_buffer.size(),
                    remaining));
            left_input.read(left_buffer.data(), requested);
            right_input.read(right_buffer.data(), requested);
            if(
                left_input.gcount() != requested
                || right_input.gcount() != requested
                || !std::equal(
                    left_buffer.begin(),
                    left_buffer.begin() + requested,
                    right_buffer.begin()))
            {
                return false;
            }
            remaining -= static_cast<std::uint64_t>(requested);
        }
        return !left_input.bad() && !right_input.bad();
    }
    catch(...)
    {
        return false;
    }
}

[[nodiscard]] constexpr auto lower_hex_digit(
    const std::uint8_t nibble) noexcept -> char
{
    return nibble < 10U
        ? static_cast<char>('0' + nibble)
        : static_cast<char>('a' + (nibble - 10U));
}

template<typename Bytes>
[[nodiscard]] auto lower_hex(const Bytes& bytes) -> std::string
{
    std::string result;
    result.reserve(bytes.size() * 2U);
    for(const auto byte : bytes)
    {
        result.push_back(lower_hex_digit(
            static_cast<std::uint8_t>(byte >> 4U)));
        result.push_back(lower_hex_digit(
            static_cast<std::uint8_t>(byte & 0x0fU)));
    }
    return result;
}

[[nodiscard]] constexpr auto hex_value(const char value) noexcept
    -> std::optional<std::uint8_t>
{
    if(value >= '0' && value <= '9')
    {
        return static_cast<std::uint8_t>(value - '0');
    }
    if(value >= 'a' && value <= 'f')
    {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    return std::nullopt;
}

template<typename Object>
[[nodiscard]] auto parse_object(
    const std::string_view text,
    const git_hash_algorithm algorithm) -> std::optional<Object>
{
    const auto byte_count =
        algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
    if(text.size() != byte_count * 2U)
    {
        return std::nullopt;
    }
    Object result{.Algorithm = algorithm};
    for(std::size_t index = 0; index < byte_count; ++index)
    {
        const auto high = hex_value(text[index * 2U]);
        const auto low = hex_value(text[index * 2U + 1U]);
        if(!high.has_value() || !low.has_value())
        {
            return std::nullopt;
        }
        result.Digest[index] = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(*high << 4U) | *low);
    }
    if(!application::valid(result))
    {
        return std::nullopt;
    }
    return result;
}

template<typename Object>
[[nodiscard]] auto object_text(const Object& object) -> std::string
{
    const auto byte_count =
        object.Algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
    return lower_hex(
        std::span{object.Digest}.first(byte_count));
}

[[nodiscard]] auto validate_installed_pack(
    const std::filesystem::path& repository,
    const git_pack_id& pack,
    const std::filesystem::path& spool,
    const std::uint64_t expected_bytes) -> bool
{
    const auto pack_directory = repository / "objects" / "pack";
    if(!private_directory(pack_directory))
    {
        return false;
    }
    const auto stem = "pack-" + object_text(pack);
    const auto installed_pack = pack_directory / (stem + ".pack");
    const auto installed_index = pack_directory / (stem + ".idx");
    const auto reverse_index = pack_directory / (stem + ".rev");
    if(
        !equal_private_files(
            spool,
            installed_pack,
            expected_bytes)
        || !private_regular_file(installed_index))
    {
        return false;
    }
    std::error_code error;
    const auto index_bytes =
        std::filesystem::file_size(installed_index, error);
    if(
        error || index_bytes == 0U
        || index_bytes > maximum_pack_index_bytes)
    {
        return false;
    }
    return !std::filesystem::exists(reverse_index, error) && !error;
}

[[nodiscard]] auto valid_hash_algorithm(
    const git_hash_algorithm algorithm) noexcept -> bool
{
    return algorithm == git_hash_algorithm::sha1
        || algorithm == git_hash_algorithm::sha256;
}

[[nodiscard]] auto valid_fixed_hex(
    const std::string_view value,
    const std::size_t size) noexcept -> bool
{
    return value.size() == size
        && std::all_of(
            value.begin(),
            value.end(),
            [](const char character)
            {
                return hex_value(character).has_value();
            });
}

[[nodiscard]] auto ordinal_text(
    const std::uint64_t ordinal) -> std::string
{
    std::array<std::uint8_t, 8U> bytes{};
    for(std::size_t index = 0; index < bytes.size(); ++index)
    {
        const auto shift =
            static_cast<unsigned int>((bytes.size() - index - 1U) * 8U);
        bytes[index] = static_cast<std::uint8_t>(ordinal >> shift);
    }
    return lower_hex(bytes);
}

[[nodiscard]] auto ref_text(
    const application::git_private_ref& ref)
    -> std::optional<std::string>
{
    return std::visit(
        [](const auto& value) -> std::optional<std::string>
        {
            using value_type = std::decay_t<decltype(value)>;
            const auto endpoint =
                value.Endpoint
                    == application::git_private_ref_endpoint::base
                ? "base"
                : value.Endpoint
                        == application::git_private_ref_endpoint::head
                    ? "head"
                    : nullptr;
            if(endpoint == nullptr)
            {
                return std::nullopt;
            }
            if constexpr(
                std::is_same_v<
                    value_type,
                    application::git_staging_ref>)
            {
                if(!application::valid_identity(value.Operation))
                {
                    return std::nullopt;
                }
                return "refs/betterpullreview/v1/stage/"
                    + lower_hex(value.Operation.Bytes) + "/" + endpoint;
            }
            else
            {
                if(
                    !application::valid_identity(value.Scope)
                    || value.Ordinal == 0U
                    || value.Ordinal
                        > static_cast<std::uint64_t>(
                            std::numeric_limits<std::int64_t>::max()))
                {
                    return std::nullopt;
                }
                return "refs/betterpullreview/v1/scope/"
                    + lower_hex(value.Scope.Bytes) + "/epoch/"
                    + ordinal_text(value.Ordinal) + "/" + endpoint;
            }
        },
        ref);
}

[[nodiscard]] auto wait_milliseconds(
    const clock::time_point deadline) noexcept -> int
{
    const auto remaining = deadline - clock::now();
    if(remaining <= clock::duration::zero())
    {
        return 0;
    }
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
    if(milliseconds < std::chrono::milliseconds{1})
    {
        return 1;
    }
    return static_cast<int>(
        std::min(milliseconds, wait_slice).count());
}

auto kill_process_group(const qint64 process_group_id) noexcept -> void
{
#if defined(Q_OS_UNIX)
    if(
        process_group_id > 1
        && process_group_id
            <= static_cast<qint64>(
                std::numeric_limits<pid_t>::max()))
    {
        static_cast<void>(::kill(
            -static_cast<pid_t>(process_group_id),
            SIGKILL));
    }
#else
    static_cast<void>(process_group_id);
#endif
}

[[nodiscard]] auto resolve_process_group(
    const QProcess& process,
    const qint64 known_process_group_id) noexcept -> qint64
{
    if(known_process_group_id > 1)
    {
        return known_process_group_id;
    }
    const auto process_id = process.processId();
    return process_id > 1 ? process_id : 0;
}

class windows_job final
{
public:
    windows_job() = default;
    windows_job(const windows_job&) = delete;
    auto operator=(const windows_job&) -> windows_job& = delete;
    ~windows_job()
    {
#if defined(Q_OS_WIN)
        if(Handle != nullptr)
        {
            static_cast<void>(CloseHandle(Handle));
        }
#endif
    }

    [[nodiscard]] auto attach(const qint64 process_id) noexcept -> bool
    {
#if defined(Q_OS_WIN)
        Handle = CreateJobObjectW(nullptr, nullptr);
        if(Handle == nullptr)
        {
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(
            SetInformationJobObject(
                Handle,
                JobObjectExtendedLimitInformation,
                &limits,
                sizeof(limits))
            == 0)
        {
            return false;
        }
        const auto process = OpenProcess(
            PROCESS_SET_QUOTA | PROCESS_TERMINATE,
            FALSE,
            static_cast<DWORD>(process_id));
        if(process == nullptr)
        {
            return false;
        }
        const auto assigned =
            AssignProcessToJobObject(Handle, process) != 0;
        static_cast<void>(CloseHandle(process));
        return assigned;
#else
        static_cast<void>(process_id);
        return true;
#endif
    }

    auto terminate() noexcept -> void
    {
#if defined(Q_OS_WIN)
        if(Handle != nullptr)
        {
            static_cast<void>(TerminateJobObject(Handle, 1U));
        }
#endif
    }

private:
#if defined(Q_OS_WIN)
    HANDLE Handle = nullptr;
#endif
};

enum class process_stream : std::uint8_t
{
    standard_output,
    standard_error,
};

[[nodiscard]] auto drain_channel(
    QProcess& process,
    const process_stream stream,
    const std::size_t limit,
    std::size_t& consumed,
    std::string* retained)
    -> std::optional<git_process_failure>
{
    const auto channel =
        stream == process_stream::standard_output
        ? QProcess::StandardOutput
        : QProcess::StandardError;
    const auto failure =
        stream == process_stream::standard_output
        ? git_process_failure::standard_output_limit
        : git_process_failure::standard_error_limit;
    process.setReadChannel(channel);

    std::array<char, 16U * 1024U> buffer{};
    while(process.bytesAvailable() > 0)
    {
        const auto remaining = limit - consumed;
        const auto requested = std::min<std::size_t>(
            buffer.size(),
            remaining + 1U);
        const auto bytes_read = process.read(
            buffer.data(),
            static_cast<qint64>(requested));
        if(bytes_read < 0)
        {
            return failure;
        }
        if(bytes_read == 0)
        {
            break;
        }
        const auto byte_count =
            static_cast<std::size_t>(bytes_read);
        if(byte_count > remaining)
        {
            return failure;
        }
        if(retained != nullptr)
        {
            retained->append(buffer.data(), byte_count);
        }
        consumed += byte_count;
    }
    return std::nullopt;
}

struct process_execution
{
    process_result Result;
    std::unique_ptr<QProcess> AwaitingReap;
    qint64 ProcessGroupId = 0;
};

[[nodiscard]] auto kill_and_reap(
    QProcess& process,
    windows_job& job,
    qint64 process_group_id) -> bool
{
    const auto deadline = clock::now() + reap_timeout;
    process_group_id =
        resolve_process_group(process, process_group_id);
    job.terminate();
    kill_process_group(process_group_id);
    if(process.state() != QProcess::NotRunning)
    {
        process.kill();
    }
    while(process.state() != QProcess::NotRunning)
    {
        const auto wait = wait_milliseconds(deadline);
        if(wait == 0)
        {
            return false;
        }
        static_cast<void>(process.waitForFinished(wait));
        job.terminate();
        process_group_id =
            resolve_process_group(process, process_group_id);
        kill_process_group(process_group_id);
        if(process.state() != QProcess::NotRunning)
        {
            process.kill();
        }
    }
    kill_process_group(process_group_id);
    return true;
}

[[nodiscard]] auto finish_background_reap(
    QProcess& process,
    const qint64 known_process_group_id) -> bool
{
    const auto deadline = clock::now() + reap_timeout;
    auto process_group_id =
        resolve_process_group(process, known_process_group_id);
    while(process.state() != QProcess::NotRunning)
    {
        const auto wait = wait_milliseconds(deadline);
        if(wait == 0)
        {
            return false;
        }
        kill_process_group(process_group_id);
        process.kill();
        static_cast<void>(process.waitForFinished(
            wait));
        process_group_id =
            resolve_process_group(process, process_group_id);
    }
    kill_process_group(process_group_id);
    return true;
}

auto abandon_unreaped_process(
    std::unique_ptr<QProcess>& process) noexcept -> void
{
    static_cast<void>(
        QObject::disconnect(
            process.get(),
            nullptr,
            process.get(),
            nullptr));
    // Destruction of a still-running QProcess may wait. The worker is already
    // permanently quarantined, so retain this one catastrophic wrapper until
    // operating-system process cleanup rather than make shutdown unbounded.
    static_cast<void>(process.release());
}

struct runner_configuration
{
    std::filesystem::path Executable;
    std::filesystem::path WorkingDirectory;
    std::filesystem::path Home;
    std::filesystem::path Xdg;
    std::filesystem::path Temporary;
    std::filesystem::path SystemConfig;
    std::filesystem::path GlobalConfig;
    std::filesystem::path Attributes;
    std::filesystem::path Templates;
    std::filesystem::path Hooks;
    std::optional<std::filesystem::path> ExecPath;
    std::optional<std::filesystem::path> WindowsSystemRoot;
};

[[nodiscard]] auto process_environment(
    const runner_configuration& configuration)
    -> QProcessEnvironment
{
    QProcessEnvironment environment;
    const auto insert =
        [&environment](
            const std::string_view name,
            const std::filesystem::path& value)
    {
        QString decoded_name;
        QString decoded_value;
        if(
            !decode_text(name, decoded_name)
            || !decode_text(path_text(value), decoded_value))
        {
            return false;
        }
        environment.insert(decoded_name, decoded_value);
        return true;
    };
    const auto insert_text =
        [&environment](
            const std::string_view name,
            const std::string_view value)
    {
        QString decoded_name;
        QString decoded_value;
        if(
            !decode_text(name, decoded_name)
            || !decode_text(value, decoded_value))
        {
            return false;
        }
        environment.insert(decoded_name, decoded_value);
        return true;
    };

    if(
        !insert("HOME", configuration.Home)
        || !insert("XDG_CONFIG_HOME", configuration.Xdg)
        || !insert("TMPDIR", configuration.Temporary)
        || !insert("TMP", configuration.Temporary)
        || !insert("TEMP", configuration.Temporary)
        || !insert("GIT_CONFIG_SYSTEM", configuration.SystemConfig)
        || !insert("GIT_CONFIG_GLOBAL", configuration.GlobalConfig)
        || !insert("GIT_ATTR_SYSTEM", configuration.Attributes)
        || !insert("GIT_TEMPLATE_DIR", configuration.Templates)
        || !insert_text("GIT_CONFIG_NOSYSTEM", "1")
        || !insert_text("GIT_ATTR_NOSYSTEM", "1")
        || !insert_text("GIT_TERMINAL_PROMPT", "0")
        || !insert_text("GIT_NO_REPLACE_OBJECTS", "1")
        || !insert_text("GIT_NO_LAZY_FETCH", "1")
        || !insert_text("GIT_OPTIONAL_LOCKS", "0")
        || !insert_text("GIT_FLUSH", "1")
        || !insert_text("LC_ALL", "C")
        || !insert_text("LANG", "C"))
    {
        return {};
    }

    const auto executable_parent =
        configuration.Executable.parent_path();
    auto search_path = executable_parent;
    if(configuration.ExecPath.has_value())
    {
        if(!insert("GIT_EXEC_PATH", *configuration.ExecPath))
        {
            return {};
        }
        search_path = *configuration.ExecPath;
        if(executable_parent != search_path)
        {
            search_path +=
#if defined(Q_OS_WIN)
                ";";
#else
                ":";
#endif
            search_path += executable_parent;
        }
    }
    if(!insert("PATH", search_path))
    {
        return {};
    }
#if defined(Q_OS_WIN)
    if(
        !configuration.WindowsSystemRoot.has_value()
        || !insert(
            "SystemRoot",
            *configuration.WindowsSystemRoot))
    {
        return {};
    }
#endif
    return environment;
}

[[nodiscard]] auto decode_arguments(
    const process_request& request,
    QStringList& arguments) -> bool
{
    if(request.Arguments.empty() || request.Arguments.size() > 64U)
    {
        return false;
    }
    for(const auto& argument : request.Arguments)
    {
        if(
            argument.empty() || argument.size() > 4096U
            || argument.find('\0') != std::string::npos)
        {
            return false;
        }
        QString decoded;
        if(!decode_text(argument, decoded))
        {
            return false;
        }
        arguments.push_back(std::move(decoded));
    }
    return true;
}

[[nodiscard]] auto failed_execution(
    std::unique_ptr<QProcess>& process,
    windows_job& job,
    const qint64 process_group_id,
    const git_process_failure failure) -> process_execution
{
    if(kill_and_reap(*process, job, process_group_id))
    {
        return {
            .Result = std::unexpected{failure},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    return {
        .Result = std::unexpected{git_process_failure::reap_failure},
        .AwaitingReap = std::move(process),
        .ProcessGroupId = process_group_id,
    };
}

[[nodiscard]] auto execute_process(
    const runner_configuration& configuration,
    const process_request& request,
    const std::stop_token stop,
    const std::atomic_bool& shutdown_requested,
    const clock::time_point deadline) -> process_execution
{
    const auto canonical_executable =
        canonical_existing(configuration.Executable);
    if(
        !canonical_executable.has_value()
        || *canonical_executable != configuration.Executable
        || !regular_executable(*canonical_executable)
        || !canonical_existing(
            configuration.WorkingDirectory).has_value()
        || !empty_private_directory(configuration.Templates)
        || !empty_private_directory(configuration.Hooks)
        || (configuration.ExecPath.has_value()
            && canonical_existing(*configuration.ExecPath)
                != configuration.ExecPath))
    {
        return {
            .Result = std::unexpected{
                git_process_failure::invalid_configuration},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    QString executable;
    QString working_directory;
    QStringList arguments;
    const auto environment = process_environment(configuration);
    if(
        !decode_text(path_text(configuration.Executable), executable)
        || !decode_text(
            path_text(configuration.WorkingDirectory),
            working_directory)
        || !decode_arguments(request, arguments)
        || environment.isEmpty())
    {
        return {
            .Result = std::unexpected{
                git_process_failure::invalid_configuration},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    if(
        stop.stop_requested()
        || shutdown_requested.load(std::memory_order_acquire))
    {
        return {
            .Result = std::unexpected{git_process_failure::cancelled},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }

    qint64 process_group_id = 0;
    auto process = std::make_unique<QProcess>();
    windows_job job;
    try
    {
        process->setProgram(executable);
        process->setArguments(arguments);
        process->setProcessEnvironment(environment);
        process->setWorkingDirectory(working_directory);
        process->setProcessChannelMode(QProcess::SeparateChannels);
        process->setInputChannelMode(QProcess::ManagedInputChannel);
#if defined(Q_OS_UNIX)
        process->setUnixProcessParameters(
            QProcess::UnixProcessFlag::CloseFileDescriptors
            | QProcess::UnixProcessFlag::DisableCoreDumps
            | QProcess::UnixProcessFlag::ResetSignalHandlers
            | QProcess::UnixProcessFlag::CreateNewSession);
        process->setChildProcessModifier(
            []
            {
                static_cast<void>(::umask(S_IRWXG | S_IRWXO));
            });
#endif
        QObject::connect(
            process.get(),
            &QProcess::started,
            process.get(),
            [
                process_pointer = process.get(),
                &process_group_id
            ]() noexcept
            {
                process_group_id =
                    process_pointer->processId();
            },
            Qt::DirectConnection);
        QObject::connect(
            process.get(),
            &QProcess::finished,
            process.get(),
            [&process_group_id](
                const int,
                const QProcess::ExitStatus) noexcept
            {
                kill_process_group(process_group_id);
            },
            Qt::DirectConnection);

        std::string standard_output;
        standard_output.reserve(standard_output_limit);
        std::size_t standard_output_bytes = 0U;
        std::size_t standard_error_bytes = 0U;
        const auto drain_streams =
            [&]() -> std::optional<git_process_failure>
        {
            if(
                const auto failure = drain_channel(
                    *process,
                    process_stream::standard_output,
                    standard_output_limit,
                    standard_output_bytes,
                    &standard_output);
                failure.has_value())
            {
                return failure;
            }
            if(
                const auto failure = drain_channel(
                    *process,
                    process_stream::standard_error,
                    standard_error_limit,
                    standard_error_bytes,
                    nullptr);
                failure.has_value())
            {
                return failure;
            }
            process->setReadChannel(QProcess::StandardOutput);
            return std::nullopt;
        };
        const auto terminal_failure =
            [&]() -> std::optional<git_process_failure>
        {
            if(
                stop.stop_requested()
                || shutdown_requested.load(std::memory_order_acquire))
            {
                return git_process_failure::cancelled;
            }
            if(clock::now() >= deadline)
            {
                return git_process_failure::timeout;
            }
            return std::nullopt;
        };
        const auto failed =
            [&](const git_process_failure failure)
            -> process_execution
        {
            std::fill(
                standard_output.begin(),
                standard_output.end(),
                '\0');
            standard_output.clear();
            return failed_execution(
                process,
                job,
                process_group_id,
                failure);
        };

        const auto start_deadline =
            std::min(deadline, clock::now() + start_timeout);
        process->start();
        for(;;)
        {
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }
            if(clock::now() >= start_deadline)
            {
                return failed(git_process_failure::process_start);
            }
            if(process->waitForStarted(
                   wait_milliseconds(start_deadline)))
            {
                break;
            }
            if(process->state() == QProcess::NotRunning)
            {
                if(
                    process->error()
                    == QProcess::FailedToStart)
                {
                    return failed(
                        git_process_failure::process_start);
                }
                break;
            }
        }
        process_group_id = resolve_process_group(*process, 0);
        if(
            process->state() != QProcess::NotRunning
            && !job.attach(process->processId()))
        {
            return failed(git_process_failure::process_start);
        }

        private_input_file input_file;
        const char* memory_data = nullptr;
        std::uint64_t input_size = 0U;
        if(const auto* memory = std::get_if<memory_input>(&request.Input))
        {
            memory_data = memory->Bytes.data();
            input_size = memory->Bytes.size();
        }
        else
        {
            const auto& file = std::get<file_input>(request.Input);
            if(
                !input_file.open(file.Path, file.Bytes))
            {
                return failed(git_process_failure::standard_input);
            }
            input_size = file.Bytes;
        }

        std::array<char, input_chunk_bytes> input_buffer{};
        std::uint64_t input_offset = 0U;
        std::size_t buffered_input_offset = 0U;
        std::size_t buffered_input_size = 0U;
        bool input_closed = false;
        while(process->state() != QProcess::NotRunning)
        {
            if(
                input_offset < input_size
                && process->bytesToWrite()
                    <= maximum_pending_input_bytes)
            {
                const auto remaining = input_size - input_offset;
                auto requested = std::min<std::uint64_t>(
                    input_chunk_bytes,
                    remaining);
                const char* bytes = nullptr;
                if(memory_data != nullptr)
                {
                    bytes = memory_data + input_offset;
                }
                else
                {
                    if(buffered_input_offset == buffered_input_size)
                    {
                        const auto received = input_file.read(
                            std::span{input_buffer}.first(
                                static_cast<std::size_t>(requested)));
                        if(
                            !received.has_value()
                            || *received == 0U)
                        {
                            return failed(
                                git_process_failure::standard_input);
                        }
                        buffered_input_offset = 0U;
                        buffered_input_size = *received;
                    }
                    requested =
                        buffered_input_size - buffered_input_offset;
                    bytes =
                        input_buffer.data() + buffered_input_offset;
                }
                const auto accepted = process->write(
                    bytes,
                    static_cast<qint64>(requested));
                if(accepted < 0)
                {
                    return failed(
                        git_process_failure::standard_input);
                }
                input_offset +=
                    static_cast<std::uint64_t>(accepted);
                if(memory_data == nullptr)
                {
                    buffered_input_offset +=
                        static_cast<std::size_t>(accepted);
                }
            }
            if(
                input_offset == input_size && !input_closed)
            {
                process->closeWriteChannel();
                input_closed = true;
            }
            if(const auto failure = drain_streams(); failure.has_value())
            {
                return failed(*failure);
            }
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }
            process->setReadChannel(QProcess::StandardOutput);
            static_cast<void>(process->waitForReadyRead(
                wait_milliseconds(deadline)));
            if(process->bytesToWrite() > 0)
            {
                static_cast<void>(
                    process->waitForBytesWritten(0));
            }
        }
        if(const auto failure = drain_streams(); failure.has_value())
        {
            return failed(*failure);
        }
        kill_process_group(process_group_id);
        if(input_offset != input_size)
        {
            return failed(git_process_failure::standard_input);
        }
        if(process->exitStatus() == QProcess::CrashExit)
        {
            return failed(git_process_failure::process_crashed);
        }
        const auto exit_code = process->exitCode();
        if(
            exit_code != 0
            && !(request.AllowExitOne && exit_code == 1))
        {
            return failed(git_process_failure::nonzero_exit);
        }
        if(standard_error_bytes != 0U)
        {
            return failed(git_process_failure::malformed_output);
        }
        return {
            .Result = process_success{
                .ExitCode = exit_code,
                .StandardOutput = std::move(standard_output),
            },
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    catch(...)
    {
        return failed_execution(
            process,
            job,
            process_group_id,
            git_process_failure::process_start);
    }
}

struct process_work_item
{
    process_request Request;
    std::stop_token Stop;
    clock::time_point Deadline;
    std::mutex Mutex;
    std::condition_variable Completed;
    std::optional<process_result> Result;
};

class process_worker final
{
public:
    explicit process_worker(runner_configuration configuration)
        : Configuration{std::move(configuration)},
          Worker{[this]
        {
            worker_loop();
        }}
    {
    }

    process_worker(const process_worker&) = delete;
    auto operator=(const process_worker&) -> process_worker& = delete;

    ~process_worker()
    {
        ShutdownRequested.store(true, std::memory_order_release);
        WorkAvailable.notify_all();
        SlotAvailable.notify_all();
        if(Worker.joinable())
        {
            Worker.join();
        }
    }

    [[nodiscard]] auto submit(
        process_request request,
        const std::stop_token stop) -> process_result
    {
        if(stop.stop_requested())
        {
            return std::unexpected{git_process_failure::cancelled};
        }
        std::shared_ptr<process_work_item> item;
        try
        {
            item = std::make_shared<process_work_item>();
            item->Request = std::move(request);
            item->Stop = stop;
            item->Deadline =
                clock::now() + item->Request.Timeout;
        }
        catch(...)
        {
            return std::unexpected{
                git_process_failure::process_start};
        }

        {
            std::unique_lock lock{Mutex};
            while(Busy)
            {
                if(
                    ShutdownRequested.load(std::memory_order_acquire)
                    || stop.stop_requested())
                {
                    return std::unexpected{
                        git_process_failure::cancelled};
                }
                if(Quarantined)
                {
                    return std::unexpected{
                        git_process_failure::reap_failure};
                }
                if(clock::now() >= item->Deadline)
                {
                    return std::unexpected{
                        git_process_failure::timeout};
                }
                SlotAvailable.wait_for(lock, wait_slice);
            }
            if(Quarantined)
            {
                return std::unexpected{
                    git_process_failure::reap_failure};
            }
            if(ShutdownRequested.load(std::memory_order_acquire))
            {
                return std::unexpected{
                    git_process_failure::cancelled};
            }
            Busy = true;
            Pending = item;
        }
        WorkAvailable.notify_one();

        std::unique_lock result_lock{item->Mutex};
        const auto completion_deadline =
            item->Deadline + reap_timeout + wait_slice;
        if(
            !item->Completed.wait_until(
                result_lock,
                completion_deadline,
                [&]
                {
                    return item->Result.has_value();
                }))
        {
            {
                std::lock_guard lock{Mutex};
                Quarantined = true;
            }
            SlotAvailable.notify_all();
            return std::unexpected{
                git_process_failure::reap_failure};
        }
        return std::move(*item->Result);
    }

private:
    auto publish(
        const std::shared_ptr<process_work_item>& item,
        process_result result) -> void
    {
        {
            std::lock_guard lock{item->Mutex};
            item->Result.emplace(std::move(result));
        }
        item->Completed.notify_one();
    }

    auto worker_loop() -> void
    {
        for(;;)
        {
            std::shared_ptr<process_work_item> item;
            {
                std::unique_lock lock{Mutex};
                WorkAvailable.wait(
                    lock,
                    [&]
                    {
                        return Pending != nullptr
                            || ShutdownRequested.load(
                                std::memory_order_acquire);
                    });
                if(Pending == nullptr)
                {
                    return;
                }
                item = std::move(Pending);
            }

            auto execution = execute_process(
                Configuration,
                item->Request,
                item->Stop,
                ShutdownRequested,
                item->Deadline);
            if(execution.AwaitingReap != nullptr)
            {
                {
                    std::lock_guard lock{Mutex};
                    Quarantined = true;
                }
                SlotAvailable.notify_all();
            }
            publish(item, std::move(execution.Result));
            bool reaped = true;
            if(execution.AwaitingReap != nullptr)
            {
                reaped = finish_background_reap(
                    *execution.AwaitingReap,
                    execution.ProcessGroupId);
                if(reaped)
                {
                    execution.AwaitingReap.reset();
                }
                else
                {
                    abandon_unreaped_process(
                        execution.AwaitingReap);
                }
            }
            {
                std::lock_guard lock{Mutex};
                Busy = false;
                Quarantined = !reaped;
            }
            SlotAvailable.notify_all();
        }
    }

    runner_configuration Configuration;
    std::mutex Mutex;
    std::condition_variable WorkAvailable;
    std::condition_variable SlotAvailable;
    std::shared_ptr<process_work_item> Pending;
    bool Busy = false;
    bool Quarantined = false;
    std::atomic_bool ShutdownRequested = false;
    std::thread Worker;
};

[[nodiscard]] auto repository_config(
    const git_hash_algorithm algorithm) -> std::string
{
    auto result = std::string{
        "[core]\n"
        "\trepositoryformatversion = 0\n"
        "\tbare = true\n"
        "\tlogallrefupdates = false\n"};
    if(algorithm == git_hash_algorithm::sha256)
    {
        result =
            "[core]\n"
            "\trepositoryformatversion = 1\n"
            "\tbare = true\n"
            "\tlogallrefupdates = false\n"
            "[extensions]\n"
            "\tobjectformat = sha256\n";
    }
    return result;
}

[[nodiscard]] auto common_git_arguments(
    const std::filesystem::path& repository,
    const std::filesystem::path& hooks) -> std::vector<std::string>
{
    return {
        "-c",
        "core.fsync=all",
        "-c",
        "core.fsyncMethod=fsync",
        "-c",
        "core.hooksPath=" + path_text(hooks),
        "-c",
        "core.fsmonitor=false",
        "-c",
        "credential.helper=",
        "-c",
        "credential.interactive=never",
        "-c",
        "protocol.allow=never",
        "-c",
        "protocol.file.allow=never",
        "-c",
        "protocol.http.allow=never",
        "-c",
        "protocol.https.allow=never",
        "-c",
        "protocol.ssh.allow=never",
        "-c",
        "protocol.git.allow=never",
        "-c",
        "protocol.ext.allow=never",
        "-c",
        "submodule.recurse=false",
        "-c",
        "diff.external=",
        "-c",
        "filter.lfs.required=false",
        "-c",
        "filter.lfs.smudge=",
        "-c",
        "filter.lfs.clean=",
        "-c",
        "gc.auto=0",
        "-c",
        "maintenance.auto=false",
        "--no-pager",
        "--no-replace-objects",
        "--git-dir=" + path_text(repository),
    };
}

template<typename... Arguments>
auto append_arguments(
    std::vector<std::string>& target,
    Arguments&&... arguments) -> void
{
    (target.emplace_back(std::forward<Arguments>(arguments)), ...);
}

class installed_git_process final
    : public application::git_process_port
{
public:
    installed_git_process(
        application::git_installation installation,
        std::filesystem::path isolation_root,
        std::unique_ptr<process_worker> worker)
        : Installation{std::move(installation)},
          IsolationRoot{std::move(isolation_root)},
          RepositoriesRoot{IsolationRoot / "repositories" / "v1"},
          IncomingRoot{IsolationRoot / "incoming" / "v1"},
          TemplatesRoot{IsolationRoot / "templates"},
          HooksRoot{IsolationRoot / "hooks"},
          Worker{std::move(worker)}
    {
    }

    [[nodiscard]] auto installation() const noexcept
        -> const application::git_installation& override
    {
        return Installation;
    }

    [[nodiscard]] auto pack_spool_path(
        const application::git_operation_identity& operation) const
        -> git_process_result<std::filesystem::path> override
    {
        if(
            !application::valid_identity(operation)
            || !private_directory(IncomingRoot))
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        return IncomingRoot
            / (lower_hex(operation.Bytes) + ".pack.part");
    }

    [[nodiscard]] auto initialize_bare_repository(
        const application::git_initialize_repository_request& request,
        const std::stop_token stop)
        -> git_process_result<void> override
    {
        if(
            !valid_hash_algorithm(request.Algorithm)
            || !valid_new_repository_path(request.Repository))
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        auto arguments = std::vector<std::string>{
            "init",
            "--bare",
            "--quiet",
            "--initial-branch=main",
            request.Algorithm == git_hash_algorithm::sha1
                ? "--object-format=sha1"
                : "--object-format=sha256",
            "--template=" + path_text(TemplatesRoot),
            path_text(request.Repository),
        };
        const auto result = submit(
            process_request{
                .Arguments = std::move(arguments),
                .Input = memory_input{},
                .Timeout = process_timeout,
            },
            stop);
        if(!result.has_value())
        {
            return std::unexpected{result.error()};
        }
        if(!result->StandardOutput.empty())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        std::error_code permission_error;
        std::filesystem::permissions(
            request.Repository,
            std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace,
            permission_error);
        const auto config = repository_config(request.Algorithm);
        if(
            permission_error
            || !write_private_file(
                request.Repository / "config",
                config)
            || !write_private_file(
                request.Repository / "HEAD",
                "ref: refs/heads/main\n")
            || !valid_repository(
                request.Repository,
                request.Algorithm))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        return {};
    }

    [[nodiscard]] auto index_pack(
        const application::git_index_pack_request& request,
        const std::stop_token stop)
        -> git_process_result<
            application::git_index_pack_success> override
    {
        if(
            !application::valid_identity(request.Operation)
            || !application::valid(request.ExpectedPack)
            || request.SpoolBytes == 0U
            || request.MaximumInputBytes < request.SpoolBytes
            || request.MaximumInputBytes
                > application::maximum_git_pack_input_bytes)
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        if(
            !valid_repository(
                request.Repository,
                request.ExpectedPack.Algorithm))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        const auto spool = pack_spool_path(request.Operation);
        if(!spool.has_value())
        {
            return std::unexpected{spool.error()};
        }
        std::error_code error;
        if(
            !private_regular_file(*spool)
            || std::filesystem::file_size(*spool, error)
                != request.SpoolBytes
            || error)
        {
            return std::unexpected{
                git_process_failure::standard_input};
        }

        auto arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "index-pack",
            "--stdin",
            "--strict",
            "--check-self-contained-and-connected",
            "--max-input-size="
                + std::to_string(request.MaximumInputBytes),
            "--threads=1",
            "--no-rev-index");
        const auto result = submit(
            process_request{
                .Arguments = std::move(arguments),
                .Input = file_input{
                    .Path = *spool,
                    .Bytes = request.SpoolBytes,
                },
                .Timeout = pack_process_timeout,
            },
            stop);
        if(!result.has_value())
        {
            return std::unexpected{result.error()};
        }
        const auto expected =
            "pack\t" + object_text(request.ExpectedPack) + "\n";
        if(result->StandardOutput != expected)
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        if(
            !validate_installed_pack(
                request.Repository,
                request.ExpectedPack,
                *spool,
                request.SpoolBytes))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        return application::git_index_pack_success{
            .Pack = request.ExpectedPack};
    }

    [[nodiscard]] auto inspect_commit(
        const application::git_commit_request& request,
        const std::stop_token stop)
        -> git_process_result<
            application::git_commit_inspection> override
    {
        if(
            !application::valid(request.Commit))
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        if(
            !valid_repository(
                request.Repository,
                request.Commit.Algorithm))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        const auto commit = object_text(request.Commit);
        auto arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "cat-file",
            "-e",
            commit + "^{commit}");
        const auto exists = submit(
            process_request{.Arguments = std::move(arguments)},
            stop);
        if(!exists.has_value())
        {
            return std::unexpected{exists.error()};
        }
        if(!exists->StandardOutput.empty())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }

        arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "rev-parse",
            "--verify",
            "--end-of-options",
            commit + "^{tree}");
        const auto tree = submit(
            process_request{.Arguments = std::move(arguments)},
            stop);
        if(!tree.has_value())
        {
            return std::unexpected{tree.error()};
        }
        if(
            tree->StandardOutput.empty()
            || tree->StandardOutput.back() != '\n')
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        const auto parsed = parse_object<git_tree_id>(
            std::string_view{tree->StandardOutput}.substr(
                0U,
                tree->StandardOutput.size() - 1U),
            request.Commit.Algorithm);
        if(!parsed.has_value())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "cat-file",
            "-e",
            object_text(*parsed) + "^{tree}");
        const auto tree_exists = submit(
            process_request{.Arguments = std::move(arguments)},
            stop);
        if(!tree_exists.has_value())
        {
            return std::unexpected{tree_exists.error()};
        }
        if(!tree_exists->StandardOutput.empty())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        return application::git_commit_inspection{
            .Tree = *parsed};
    }

    [[nodiscard]] auto is_ancestor(
        const application::git_ancestry_request& request,
        const std::stop_token stop)
        -> git_process_result<git_ancestry> override
    {
        if(
            !application::valid(request.Ancestor)
            || !application::valid(request.Descendant)
            || request.Ancestor.Algorithm
                != request.Descendant.Algorithm)
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        if(
            !valid_repository(
                request.Repository,
                request.Ancestor.Algorithm))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        auto arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "merge-base",
            "--is-ancestor",
            object_text(request.Ancestor),
            object_text(request.Descendant));
        const auto result = submit(
            process_request{
                .Arguments = std::move(arguments),
                .AllowExitOne = true,
            },
            stop);
        if(!result.has_value())
        {
            return std::unexpected{result.error()};
        }
        if(!result->StandardOutput.empty())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        return result->ExitCode == 0
            ? git_ancestry::ancestor
            : git_ancestry::not_ancestor;
    }

    [[nodiscard]] auto update_refs(
        const application::git_update_refs_request& request,
        const std::stop_token stop)
        -> git_process_result<void> override
    {
        const auto algorithm = mutation_algorithm(request.Mutations);
        if(
            !algorithm.has_value()
            || request.Mutations.empty()
            || request.Mutations.size()
                > application::maximum_git_ref_transaction_updates)
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        if(!valid_repository(request.Repository, *algorithm))
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        auto input = std::string{"start"};
        input.push_back('\0');
        for(const auto& mutation : request.Mutations)
        {
            const auto appended = std::visit(
                [&](const auto& value)
                {
                    const auto ref = ref_text(value.Ref);
                    if(!ref.has_value())
                    {
                        return false;
                    }
                    using value_type =
                        std::decay_t<decltype(value)>;
                    if constexpr(
                        std::is_same_v<
                            value_type,
                            application::git_ref_write>)
                    {
                        if(
                            !application::valid(value.DesiredObject)
                            || value.DesiredObject.Algorithm
                                != *algorithm)
                        {
                            return false;
                        }
                        if(
                            std::holds_alternative<
                                application::git_ref_expected_absent>(
                                value.ExpectedOld))
                        {
                            input += "create " + *ref;
                            input.push_back('\0');
                            input += object_text(value.DesiredObject);
                            input.push_back('\0');
                            return true;
                        }
                        const auto expected =
                            std::get<
                                application::git_ref_expected_object>(
                                value.ExpectedOld)
                                .Object;
                        if(
                            !application::valid(expected)
                            || expected.Algorithm != *algorithm)
                        {
                            return false;
                        }
                        input += "update " + *ref;
                        input.push_back('\0');
                        input += object_text(value.DesiredObject);
                        input.push_back('\0');
                        input += object_text(expected);
                        input.push_back('\0');
                        return true;
                    }
                    else
                    {
                        if(
                            !application::valid(value.ExpectedOld)
                            || value.ExpectedOld.Algorithm
                                != *algorithm)
                        {
                            return false;
                        }
                        input += "delete " + *ref;
                        input.push_back('\0');
                        input += object_text(value.ExpectedOld);
                        input.push_back('\0');
                        return true;
                    }
                },
                mutation);
            if(!appended)
            {
                return std::unexpected{
                    git_process_failure::invalid_request};
            }
        }
        input += "prepare";
        input.push_back('\0');
        input += "commit";
        input.push_back('\0');

        auto arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "update-ref",
            "--stdin",
            "-z",
            "--no-deref");
        const auto result = submit(
            process_request{
                .Arguments = std::move(arguments),
                .Input = memory_input{.Bytes = std::move(input)},
            },
            stop);
        if(!result.has_value())
        {
            return std::unexpected{result.error()};
        }
        if(result->StandardOutput != transaction_success)
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        return {};
    }

    [[nodiscard]] auto inspect_ref(
        const application::git_inspect_ref_request& request,
        const std::stop_token stop)
        -> git_process_result<std::optional<git_commit_id>> override
    {
        const auto ref = ref_text(request.Ref);
        if(!ref.has_value())
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        const auto algorithm = repository_algorithm(request.Repository);
        if(!algorithm.has_value())
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        auto arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "show-ref",
            "--verify",
            "--quiet",
            *ref);
        const auto presence = submit(
            process_request{
                .Arguments = std::move(arguments),
                .AllowExitOne = true,
            },
            stop);
        if(!presence.has_value())
        {
            return std::unexpected{presence.error()};
        }
        if(!presence->StandardOutput.empty())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        if(presence->ExitCode == 1)
        {
            return std::optional<git_commit_id>{};
        }
        arguments =
            common_git_arguments(request.Repository, HooksRoot);
        append_arguments(
            arguments,
            "show-ref",
            "--verify",
            "--hash",
            *ref);
        const auto result = submit(
            process_request{.Arguments = std::move(arguments)},
            stop);
        if(!result.has_value())
        {
            return std::unexpected{result.error()};
        }
        if(
            result->StandardOutput.empty()
            || result->StandardOutput.back() != '\n')
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        const auto object = parse_object<git_commit_id>(
            std::string_view{result->StandardOutput}.substr(
                0U,
                result->StandardOutput.size() - 1U),
            *algorithm);
        if(!object.has_value())
        {
            return std::unexpected{
                git_process_failure::malformed_output};
        }
        const auto inspection = inspect_commit(
            application::git_commit_request{
                .Repository = request.Repository,
                .Commit = *object,
            },
            stop);
        if(!inspection.has_value())
        {
            return std::unexpected{inspection.error()};
        }
        return std::optional<git_commit_id>{*object};
    }

    [[nodiscard]] auto capability_probe()
        -> git_process_result<void>;

private:
    [[nodiscard]] auto submit(
        process_request request,
        const std::stop_token stop) -> process_result
    {
        if(
            !empty_private_directory(TemplatesRoot)
            || !empty_private_directory(HooksRoot))
        {
            return std::unexpected{
                git_process_failure::invalid_configuration};
        }
        return Worker->submit(std::move(request), stop);
    }

    [[nodiscard]] auto valid_new_repository_path(
        const std::filesystem::path& repository) const -> bool
    {
        if(
            repository.empty() || !repository.is_absolute()
            || repository.parent_path() != RepositoriesRoot)
        {
            return false;
        }
        const auto name = repository.filename().string();
        if(
            name.size() != 68U
            || name.substr(name.size() - 4U) != ".git"
            || !valid_fixed_hex(name.substr(0U, 64U), 64U))
        {
            return false;
        }
        std::error_code error;
        return !std::filesystem::exists(repository, error) && !error
            && private_directory(RepositoriesRoot);
    }

    [[nodiscard]] auto repository_algorithm(
        const std::filesystem::path& repository) const
        -> std::optional<git_hash_algorithm>
    {
        if(valid_repository(repository, git_hash_algorithm::sha1))
        {
            return git_hash_algorithm::sha1;
        }
        if(valid_repository(repository, git_hash_algorithm::sha256))
        {
            return git_hash_algorithm::sha256;
        }
        return std::nullopt;
    }

    [[nodiscard]] auto valid_repository(
        const std::filesystem::path& repository,
        const git_hash_algorithm algorithm) const -> bool
    {
        if(
            !valid_hash_algorithm(algorithm)
            || repository.parent_path() != RepositoriesRoot)
        {
            return false;
        }
        const auto canonical = canonical_existing(repository);
        if(!canonical.has_value() || !private_directory(*canonical))
        {
            return false;
        }
        const auto name = repository.filename().string();
        if(
            name.size() != 68U
            || name.substr(name.size() - 4U) != ".git"
            || !valid_fixed_hex(name.substr(0U, 64U), 64U))
        {
            return false;
        }
        std::error_code error;
        if(
            !private_directory(repository / "objects")
            || !private_directory(repository / "refs")
            || !read_exact_file(
                repository / "config",
                repository_config(algorithm))
            || !read_exact_file(
                repository / "HEAD",
                "ref: refs/heads/main\n")
            || std::filesystem::exists(
                repository / "objects" / "info" / "alternates",
                error)
            || error
            || std::filesystem::exists(
                repository
                    / "objects" / "info" / "http-alternates",
                error)
            || error
            || std::filesystem::exists(
                repository / "info" / "grafts",
                error)
            || error
            || std::filesystem::exists(
                repository / "refs" / "replace",
                error)
            || error
            || std::filesystem::exists(
                repository / "shallow",
                error)
            || error
            || std::filesystem::exists(
                repository / "worktrees",
                error)
            || error)
        {
            return false;
        }
        return true;
    }

    [[nodiscard]] static auto mutation_algorithm(
        const std::span<const application::git_ref_mutation> mutations)
        -> std::optional<git_hash_algorithm>
    {
        if(mutations.empty())
        {
            return std::nullopt;
        }
        return std::visit(
            [](const auto& mutation)
                -> std::optional<git_hash_algorithm>
            {
                using mutation_type =
                    std::decay_t<decltype(mutation)>;
                if constexpr(
                    std::is_same_v<
                        mutation_type,
                        application::git_ref_write>)
                {
                    return mutation.DesiredObject.Algorithm;
                }
                else
                {
                    return mutation.ExpectedOld.Algorithm;
                }
            },
            mutations.front());
    }

    application::git_installation Installation;
    std::filesystem::path IsolationRoot;
    std::filesystem::path RepositoriesRoot;
    std::filesystem::path IncomingRoot;
    std::filesystem::path TemplatesRoot;
    std::filesystem::path HooksRoot;
    std::unique_ptr<process_worker> Worker;
};

inline constexpr std::array<std::uint8_t, 230U> probe_pack{
    0x50, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    0x97, 0x0b, 0x78, 0x9c, 0x95, 0xcb, 0xc1, 0x0a, 0x02, 0x21, 0x10, 0x80,
    0xe1, 0xbb, 0x4f, 0x31, 0xf7, 0x20, 0x74, 0x35, 0xdb, 0x85, 0x58, 0xa2,
    0x4b, 0xd7, 0xe8, 0x0d, 0x46, 0x1d, 0x69, 0xc0, 0x5d, 0xc5, 0xa6, 0xa8,
    0xb7, 0xaf, 0x1e, 0xa1, 0xff, 0xfc, 0xfd, 0xd2, 0x89, 0xc0, 0x45, 0x9a,
    0x72, 0x0e, 0x79, 0x1f, 0x63, 0xb2, 0x26, 0x51, 0x08, 0x1a, 0xad, 0xb3,
    0x7a, 0x18, 0x8c, 0x35, 0x59, 0xe7, 0x44, 0xde, 0x91, 0xdd, 0x25, 0x85,
    0x0f, 0xb9, 0xd5, 0x0e, 0xa7, 0xcb, 0x15, 0x0e, 0xa1, 0xf5, 0x23, 0xbd,
    0x70, 0x69, 0x85, 0xb6, 0xbc, 0x3e, 0xb1, 0x70, 0x9a, 0x61, 0x72, 0xde,
    0x8f, 0x6e, 0xd4, 0x1a, 0x36, 0xfa, 0x9b, 0x8a, 0x75, 0x59, 0x58, 0x84,
    0xfe, 0x79, 0xd4, 0x8f, 0xf2, 0x7a, 0x17, 0x2c, 0x85, 0x12, 0x9c, 0x59,
    0x20, 0x62, 0xc3, 0xc0, 0x85, 0xe5, 0x0d, 0xad, 0xd7, 0x40, 0xea, 0x03,
    0x67, 0xd5, 0x39, 0x11, 0xa5, 0x02, 0x78, 0x9c, 0x33, 0x34, 0x30, 0x30,
    0x33, 0x31, 0x51, 0x28, 0x28, 0xca, 0x4f, 0x4a, 0xd5, 0x2b, 0xa9, 0x28,
    0x61, 0xb8, 0xc5, 0xe3, 0xb7, 0xe3, 0xe6, 0x92, 0x5e, 0x71, 0x29, 0xe3,
    0x70, 0x6f, 0x0b, 0xf6, 0xa0, 0x87, 0xcd, 0x1a, 0xe9, 0x81, 0x00, 0xe4,
    0xb2, 0x0c, 0xc7, 0x36, 0x78, 0x9c, 0x2b, 0x28, 0xca, 0x4f, 0x4a, 0xe5,
    0x02, 0x00, 0x08, 0x96, 0x02, 0x23, 0xe2, 0x48, 0xb0, 0x8f, 0x09, 0xa7,
    0x51, 0x16, 0xfd, 0xb2, 0x39, 0x7c, 0x92, 0x8a, 0x84, 0xf6, 0xa9, 0x3f,
    0x6c, 0x34,
};

inline constexpr std::array<std::uint8_t, 271U> probe_pack_sha256{
    0x50, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    0x9f, 0x0c, 0x78, 0x9c, 0x95, 0xcb, 0x41, 0x0a, 0xc2, 0x30, 0x10, 0x40,
    0xd1, 0x7d, 0x4e, 0x31, 0x7b, 0x41, 0x92, 0x36, 0x0d, 0x13, 0x10, 0x29,
    0x6e, 0xdc, 0x8a, 0x37, 0x98, 0x24, 0x33, 0x18, 0x48, 0xdb, 0x10, 0xa3,
    0xe8, 0xed, 0xd5, 0x23, 0xf8, 0xd7, 0xef, 0xf7, 0xc6, 0x0c, 0xc9, 0x04,
    0x43, 0xe8, 0xc8, 0x85, 0xc9, 0xe0, 0x30, 0x68, 0x14, 0xd1, 0xa3, 0x9d,
    0x44, 0x9c, 0x38, 0x9b, 0x7c, 0xb4, 0x18, 0x7c, 0xd2, 0x13, 0xb2, 0x66,
    0x24, 0x44, 0x09, 0xc4, 0x12, 0xc5, 0xf8, 0x61, 0x44, 0x4b, 0x84, 0x49,
    0xd1, 0xa3, 0xdf, 0xb6, 0x06, 0xa7, 0xcb, 0x15, 0x0e, 0xa1, 0xb6, 0x99,
    0x5f, 0xb4, 0xd4, 0xc2, 0xfb, 0xbc, 0x3e, 0xa9, 0xe4, 0x74, 0x04, 0x6f,
    0x9d, 0x43, 0x8b, 0x5a, 0xc3, 0x4e, 0x7f, 0x53, 0x71, 0x5b, 0x96, 0xdc,
    0x3b, 0xff, 0xf3, 0xa8, 0x1f, 0xcd, 0xeb, 0xbd, 0x53, 0x29, 0x9c, 0xe0,
    0x9c, 0x3b, 0x44, 0xaa, 0x14, 0x72, 0xc9, 0xfd, 0x0d, 0xb5, 0x6d, 0x81,
    0xd5, 0x07, 0x40, 0x7d, 0x3f, 0x67, 0xa1, 0x03, 0x78, 0x9c, 0x01, 0x31,
    0x00, 0xce, 0xff, 0x31, 0x30, 0x30, 0x36, 0x34, 0x34, 0x20, 0x70, 0x72,
    0x6f, 0x62, 0x65, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x73, 0x32, 0x45, 0xb5,
    0xd4, 0x45, 0x55, 0x8d, 0x5e, 0xd9, 0xc6, 0x98, 0x92, 0xdb, 0x5c, 0xac,
    0xb1, 0x8a, 0x05, 0xc6, 0xde, 0x8f, 0x70, 0xd8, 0xf1, 0x3b, 0x76, 0x2b,
    0x3d, 0xa2, 0x20, 0xa1, 0xde, 0xfa, 0x15, 0xc2, 0x36, 0x78, 0x9c, 0x2b,
    0x28, 0xca, 0x4f, 0x4a, 0xe5, 0x02, 0x00, 0x08, 0x96, 0x02, 0x23, 0x3e,
    0x34, 0x68, 0x38, 0x8a, 0x66, 0xc6, 0x47, 0x5d, 0x2c, 0x1f, 0xdb, 0x8f,
    0xb7, 0x28, 0x6b, 0xc0, 0xae, 0x1f, 0x93, 0x4b, 0x63, 0x52, 0x35, 0x8c,
    0xc9, 0x8e, 0x4e, 0x5e, 0x82, 0x3b, 0x69,
};

[[nodiscard]] auto probe_identity(
    const std::string_view hex,
    const git_hash_algorithm algorithm) -> application::git_commit_id
{
    return *parse_object<application::git_commit_id>(
        hex,
        algorithm);
}

[[nodiscard]] auto probe_pack_identity(
    const std::string_view hex,
    const git_hash_algorithm algorithm) -> application::git_pack_id
{
    return *parse_object<application::git_pack_id>(
        hex,
        algorithm);
}

[[nodiscard]] auto probe_tree_identity(
    const std::string_view hex,
    const git_hash_algorithm algorithm) -> application::git_tree_id
{
    return *parse_object<application::git_tree_id>(
        hex,
        algorithm);
}

auto installed_git_process::capability_probe()
    -> git_process_result<void>
{
    const auto probe =
        [&](const git_hash_algorithm algorithm,
            const std::span<const std::uint8_t> pack_bytes,
            const std::string_view pack_hex,
            const std::string_view commit_hex,
            const std::string_view tree_hex)
            -> git_process_result<void>
    {
        application::git_operation_identity operation{};
        std::random_device random;
        for(auto& byte : operation.Bytes)
        {
            byte = static_cast<std::uint8_t>(random());
        }
        if(!application::valid_identity(operation))
        {
            operation.Bytes.front() = 1U;
        }
        const auto repository =
            RepositoriesRoot / (lower_hex(operation.Bytes) + ".git");
        const auto initialized = initialize_bare_repository(
            {.Repository = repository, .Algorithm = algorithm},
            {});
        if(!initialized.has_value())
        {
            return std::unexpected{initialized.error()};
        }
        const auto spool = pack_spool_path(operation);
        if(
            !spool.has_value()
            || !write_private_file(
                *spool,
                std::as_bytes(pack_bytes)))
        {
            return std::unexpected{
                git_process_failure::standard_input};
        }
        const auto pack =
            probe_pack_identity(pack_hex, algorithm);
        const auto commit =
            probe_identity(commit_hex, algorithm);
        const auto indexed = index_pack(
            {
                .Repository = repository,
                .Operation = operation,
                .SpoolBytes = pack_bytes.size(),
                .MaximumInputBytes = pack_bytes.size(),
                .ExpectedPack = pack,
            },
            {});
        const auto inspected = indexed.has_value()
            ? inspect_commit(
                {.Repository = repository, .Commit = commit},
                {})
            : git_process_result<
                application::git_commit_inspection>{
                std::unexpected{
                    git_process_failure::unsupported_git}};
        const auto ancestry = inspected.has_value()
            ? is_ancestor(
                {
                    .Repository = repository,
                    .Ancestor = commit,
                    .Descendant = commit,
                },
                {})
            : git_process_result<git_ancestry>{
                std::unexpected{
                    git_process_failure::unsupported_git}};
        const application::git_staging_ref probe_ref{
            .Operation = operation,
            .Endpoint =
                application::git_private_ref_endpoint::base,
        };
        const std::array<
            application::git_ref_mutation,
            1U> create{
            application::git_ref_write{
                .Ref = probe_ref,
                .DesiredObject = commit,
                .ExpectedOld =
                    application::git_ref_expected_absent{},
            }};
        const auto created =
            ancestry == git_ancestry::ancestor
            && inspected->Tree
                == probe_tree_identity(tree_hex, algorithm)
            ? update_refs(
                {.Repository = repository, .Mutations = create},
                {})
            : git_process_result<void>{
                std::unexpected{
                    git_process_failure::unsupported_git}};
        const auto observed = created.has_value()
            ? inspect_ref(
                {.Repository = repository, .Ref = probe_ref},
                {})
            : git_process_result<
                std::optional<git_commit_id>>{
                std::unexpected{
                    git_process_failure::unsupported_git}};
        const std::array<
            application::git_ref_mutation,
            1U> remove{
            application::git_ref_remove{
                .Ref = probe_ref,
                .ExpectedOld = commit,
            }};
        const auto removed =
            observed.has_value() && *observed == commit
            ? update_refs(
                {.Repository = repository, .Mutations = remove},
                {})
            : git_process_result<void>{
                std::unexpected{
                    git_process_failure::unsupported_git}};
        const auto absent = removed.has_value()
            ? inspect_ref(
                {.Repository = repository, .Ref = probe_ref},
                {})
            : git_process_result<
                std::optional<git_commit_id>>{
                std::unexpected{
                    git_process_failure::unsupported_git}};

        std::error_code cleanup_error;
        std::filesystem::remove(*spool, cleanup_error);
        cleanup_error.clear();
        std::filesystem::remove_all(repository, cleanup_error);
        if(!indexed.has_value())
        {
            return std::unexpected{indexed.error()};
        }
        if(!inspected.has_value())
        {
            return std::unexpected{inspected.error()};
        }
        if(!ancestry.has_value())
        {
            return std::unexpected{ancestry.error()};
        }
        if(!created.has_value())
        {
            return std::unexpected{created.error()};
        }
        if(!observed.has_value())
        {
            return std::unexpected{observed.error()};
        }
        if(!removed.has_value())
        {
            return std::unexpected{removed.error()};
        }
        if(!absent.has_value())
        {
            return std::unexpected{absent.error()};
        }
        if(absent->has_value())
        {
            return std::unexpected{
                git_process_failure::invalid_request};
        }
        if(cleanup_error)
        {
            return std::unexpected{
                git_process_failure::invalid_repository};
        }
        return {};
    };

    const auto sha1 = probe(
        git_hash_algorithm::sha1,
        probe_pack,
        "e248b08f09a75116fdb2397c928a84f6a93f6c34",
        "826bb464b84352d60b3d7c56481a599a838d998d",
        "4ce9ffbf7ccd31debb0a343022131f0fde64e35d");
    if(!sha1.has_value())
    {
        return sha1;
    }
    return probe(
        git_hash_algorithm::sha256,
        probe_pack_sha256,
        "3e3468388a66c6475d2c1fdb8fb7286bc0ae1f934b6352358cc98e4e5e823b69",
        "e4cd01b387db796924a8a2a9e819f44f8b6f8501c86c6a66eabbc334d751dbb8",
        "d1b1a86a6b5182208ff0345ff6f64d9c48b9d058e0e8a88fbaefcf192384aa8d");
}

[[nodiscard]] auto parse_version(
    const std::string_view output)
    -> std::optional<std::array<std::uint32_t, 3U>>
{
    constexpr std::string_view prefix = "git version ";
    if(!output.starts_with(prefix) || output.back() != '\n')
    {
        return std::nullopt;
    }
    auto cursor = prefix.size();
    std::array<std::uint32_t, 3U> version{};
    for(std::size_t component = 0U; component < version.size(); ++component)
    {
        if(cursor >= output.size())
        {
            return std::nullopt;
        }
        std::uint64_t value = 0U;
        const auto start = cursor;
        while(
            cursor < output.size()
            && output[cursor] >= '0' && output[cursor] <= '9')
        {
            value = value * 10U
                + static_cast<std::uint64_t>(
                    output[cursor] - '0');
            if(value > std::numeric_limits<std::uint32_t>::max())
            {
                return std::nullopt;
            }
            ++cursor;
        }
        if(cursor == start)
        {
            return std::nullopt;
        }
        version[component] = static_cast<std::uint32_t>(value);
        if(component + 1U < version.size())
        {
            if(cursor >= output.size() || output[cursor] != '.')
            {
                return std::nullopt;
            }
            ++cursor;
        }
    }
    if(cursor >= output.size())
    {
        return std::nullopt;
    }
    if(output[cursor] == '\n')
    {
        return cursor + 1U == output.size()
            ? std::optional{version}
            : std::nullopt;
    }
    const auto suffix = output.substr(
        cursor,
        output.size() - cursor - 1U);
    if(output.back() != '\n')
    {
        return std::nullopt;
    }
    const auto decimal =
        [](const std::string_view value)
        {
            return !value.empty()
                && std::all_of(
                    value.begin(),
                    value.end(),
                    [](const char character)
                    {
                        return character >= '0'
                            && character <= '9';
                    });
        };
    constexpr std::string_view apple_prefix = " (Apple Git-";
    if(
        suffix.starts_with(apple_prefix)
        && suffix.ends_with(')')
        && decimal(suffix.substr(
            apple_prefix.size(),
            suffix.size() - apple_prefix.size() - 1U)))
    {
        return version;
    }
    constexpr std::string_view windows_prefix = ".windows.";
    if(
        suffix.starts_with(windows_prefix)
        && decimal(suffix.substr(windows_prefix.size())))
    {
        return version;
    }
    return std::nullopt;
}

[[nodiscard]] auto supported_version(
    const std::array<std::uint32_t, 3U>& version) noexcept -> bool
{
    return version[0] > 2U
        || (version[0] == 2U && version[1] >= 39U);
}

[[nodiscard]] auto prepare_isolation(
    const std::filesystem::path& isolation) -> bool
{
    if(
        !canonical_existing(isolation).has_value()
        || !private_directory(isolation))
    {
        return false;
    }
    const std::array directories{
        isolation / "home",
        isolation / "xdg",
        isolation / "tmp",
        isolation / "work",
        isolation / "templates",
        isolation / "hooks",
        isolation / "incoming",
        isolation / "incoming" / "v1",
        isolation / "repositories",
        isolation / "repositories" / "v1",
    };
    const std::array files{
        isolation / "config-system",
        isolation / "config-global",
        isolation / "attributes-system",
    };
    const std::array empty_policy_directories{
        isolation / "templates",
        isolation / "hooks",
    };

    const auto missing =
        [](const std::filesystem::path& path)
    {
        std::error_code error;
        const auto status =
            std::filesystem::symlink_status(path, error);
        return (
                   !error
                   && status.type()
                       == std::filesystem::file_type::not_found)
            || error
                == std::make_error_code(
                    std::errc::no_such_file_or_directory);
    };
    for(const auto& directory : directories)
    {
        if(!missing(directory) && !private_directory(directory))
        {
            return false;
        }
    }
    for(const auto& file : files)
    {
        if(!missing(file) && !read_exact_file(file, {}))
        {
            return false;
        }
    }
    for(const auto& directory : empty_policy_directories)
    {
        if(!missing(directory) && !empty_private_directory(directory))
        {
            return false;
        }
    }

    for(const auto& directory : directories)
    {
        if(!make_private_directory(directory))
        {
            return false;
        }
    }
    for(const auto& file : files)
    {
        if(!write_private_file(file, std::string_view{}))
        {
            return false;
        }
    }
    return std::all_of(
        empty_policy_directories.begin(),
        empty_policy_directories.end(),
        empty_private_directory);
}

[[nodiscard]] auto runner_config(
    const std::filesystem::path& executable,
    const std::filesystem::path& isolation,
    std::optional<std::filesystem::path> exec_path,
    const std::optional<std::filesystem::path>& windows_system_root)
    -> runner_configuration
{
    return {
        .Executable = executable,
        .WorkingDirectory = isolation / "work",
        .Home = isolation / "home",
        .Xdg = isolation / "xdg",
        .Temporary = isolation / "tmp",
        .SystemConfig = isolation / "config-system",
        .GlobalConfig = isolation / "config-global",
        .Attributes = isolation / "attributes-system",
        .Templates = isolation / "templates",
        .Hooks = isolation / "hooks",
        .ExecPath = std::move(exec_path),
        .WindowsSystemRoot = windows_system_root,
    };
}
}

auto make_installed_git_process(
    const installed_git_process_configuration& configuration)
    -> std::expected<
        std::unique_ptr<application::git_process_port>,
        application::git_process_failure>
{
    try
    {
        const auto isolation =
            canonical_existing(configuration.IsolationRoot);
        const auto candidate =
            canonical_existing(configuration.GitExecutable);
        if(
            !isolation.has_value() || !candidate.has_value()
            || !private_directory(*isolation)
            || !regular_executable(*candidate)
            || !prepare_isolation(*isolation))
        {
            return std::unexpected{
                application::git_process_failure::
                    invalid_configuration};
        }
#if defined(Q_OS_WIN)
        const auto system_root =
            configuration.WindowsSystemRoot.has_value()
            ? canonical_existing(*configuration.WindowsSystemRoot)
            : std::nullopt;
        if(!system_root.has_value())
        {
            return std::unexpected{
                application::git_process_failure::
                    invalid_configuration};
        }
#else
        const auto system_root =
            std::optional<std::filesystem::path>{};
#endif

        auto bootstrap = process_worker{
            runner_config(
                *candidate,
                *isolation,
                std::nullopt,
                system_root)};
        const auto exec_path_result = bootstrap.submit(
            process_request{
                .Arguments = {"--exec-path"},
                .Input = memory_input{},
                .Timeout = process_timeout,
            },
            {});
        if(!exec_path_result.has_value())
        {
            return std::unexpected{exec_path_result.error()};
        }
        if(
            exec_path_result->StandardOutput.empty()
            || exec_path_result->StandardOutput.back() != '\n')
        {
            return std::unexpected{
                application::git_process_failure::unsupported_git};
        }
        const auto exec_path_text =
            std::string_view{exec_path_result->StandardOutput}.substr(
                0U,
                exec_path_result->StandardOutput.size() - 1U);
        if(
            exec_path_text.empty()
            || exec_path_text.find('\n') != std::string_view::npos
            || exec_path_text.find('\r') != std::string_view::npos)
        {
            return std::unexpected{
                application::git_process_failure::standard_output_limit};
        }
        const auto exec_path =
            canonical_existing(
                std::filesystem::path{exec_path_text});
        if(!exec_path.has_value())
        {
            return std::unexpected{
                application::git_process_failure::invalid_configuration};
        }
        const auto resolved_git =
            resolve_canonical_existing(
                *exec_path
#if defined(Q_OS_WIN)
                / "git.exe"
#else
                / "git"
#endif
            );
        if(
            !resolved_git.has_value()
            || !regular_executable(*resolved_git))
        {
            return std::unexpected{
                application::git_process_failure::unsupported_git};
        }

        auto worker = std::make_unique<process_worker>(
            runner_config(
                *resolved_git,
                *isolation,
                *exec_path,
                system_root));
        const auto matching_exec_path = worker->submit(
            process_request{
                .Arguments = {"--exec-path"},
                .Input = memory_input{},
                .Timeout = process_timeout,
            },
            {});
        const auto version_result = worker->submit(
            process_request{
                .Arguments = {"--version"},
                .Input = memory_input{},
                .Timeout = process_timeout,
            },
            {});
        const auto expected_exec_path =
            path_text(*exec_path) + "\n";
        if(!matching_exec_path.has_value())
        {
            return std::unexpected{matching_exec_path.error()};
        }
        if(!version_result.has_value())
        {
            return std::unexpected{version_result.error()};
        }
        if(
            matching_exec_path->StandardOutput
            != expected_exec_path)
        {
            return std::unexpected{
                application::git_process_failure::unsupported_git};
        }
        const auto version =
            parse_version(version_result->StandardOutput);
        if(!version.has_value() || !supported_version(*version))
        {
            return std::unexpected{
                application::git_process_failure::unsupported_git};
        }

        auto implementation =
            std::make_unique<installed_git_process>(
                application::git_installation{
                    .VersionMajor = (*version)[0],
                    .VersionMinor = (*version)[1],
                    .VersionPatch = (*version)[2],
                },
                *isolation,
                std::move(worker));
        const auto probe = implementation->capability_probe();
        if(!probe.has_value())
        {
            return std::unexpected{
                application::git_process_failure::unsupported_git};
        }
        return std::unique_ptr<application::git_process_port>{
            std::move(implementation)};
    }
    catch(...)
    {
        return std::unexpected{
            application::git_process_failure::invalid_configuration};
    }
}
}
