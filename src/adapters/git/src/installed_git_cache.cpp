#include "bpr/adapters/git/installed_git_cache.hpp"

#include "bpr/adapters/git/installed_git_process.hpp"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QRandomGenerator>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(Q_OS_MACOS)
#include <stdio.h>
#endif

#if defined(Q_OS_LINUX)
#include <linux/fs.h>
#include <sys/syscall.h>
#endif

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace bpr::adapters::git
{
namespace
{
namespace application = bpr::application;
namespace core = bpr::core;

inline constexpr std::string_view manifest_name =
    "betterpullreview-cache-manifest-v1";
inline constexpr std::array<std::uint8_t, 12U> manifest_magic{
    0x42U,
    0x50U,
    0x52U,
    0x43U,
    0x41U,
    0x43U,
    0x48U,
    0x45U,
    0x00U,
    0x00U,
    0x00U,
    0x01U,
};
inline constexpr std::size_t maximum_repository_entries = 1'000'000U;
inline constexpr std::uint64_t maximum_pack_index_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;
#if defined(Q_OS_UNIX)
inline constexpr std::uint8_t artifact_identity_scheme = 1U;
#elif defined(Q_OS_WIN)
inline constexpr std::uint8_t artifact_identity_scheme = 2U;
#else
inline constexpr std::uint8_t artifact_identity_scheme = 0U;
#endif
inline constexpr std::array<std::uint8_t, 12U> deletion_tombstone_magic{
    0x42U,
    0x50U,
    0x52U,
    0x44U,
    0x45U,
    0x4cU,
    0x45U,
    0x54U,
    0x45U,
    0x00U,
    0x00U,
    0x02U,
};

[[nodiscard]] constexpr auto fits_within(
    const std::uint64_t current,
    const std::uint64_t addition,
    const std::uint64_t limit) noexcept -> bool
{
    return current <= limit && addition <= limit - current;
}

[[nodiscard]] constexpr auto ref_pair_bytes(
    const core::git_hash_algorithm algorithm) noexcept -> std::uint64_t
{
    return algorithm == core::git_hash_algorithm::sha1 ? 82U : 130U;
}

[[nodiscard]] auto maximum_index_bytes(
    const core::git_hash_algorithm algorithm,
    const std::span<const std::uint8_t> header)
    -> std::optional<std::uint64_t>
{
    if(header.size() != 12U)
    {
        return std::nullopt;
    }
    const auto object_count =
        (static_cast<std::uint64_t>(header[8U]) << 24U)
        | (static_cast<std::uint64_t>(header[9U]) << 16U)
        | (static_cast<std::uint64_t>(header[10U]) << 8U)
        | static_cast<std::uint64_t>(header[11U]);
    const auto digest_bytes =
        algorithm == core::git_hash_algorithm::sha1
        ? 20U
        : algorithm == core::git_hash_algorithm::sha256 ? 32U : 0U;
    if(digest_bytes == 0U)
    {
        return std::nullopt;
    }

    // Git index v2: header, 256-entry fanout, object IDs, CRCs, 32-bit
    // offsets, a worst-case 64-bit offset for every object, and two hashes.
    const auto fixed_bytes = 8ULL + 256ULL * 4ULL + 2ULL * digest_bytes;
    const auto per_object_bytes = digest_bytes + 4ULL + 4ULL + 8ULL;
    if(
        object_count
            > (std::numeric_limits<std::uint64_t>::max() - fixed_bytes)
                / per_object_bytes)
    {
        return std::nullopt;
    }
    const auto result = fixed_bytes + object_count * per_object_bytes;
    return result <= maximum_pack_index_bytes
        ? std::optional<std::uint64_t>{result}
        : std::nullopt;
}

[[nodiscard]] auto lower_hex(
    const std::span<const std::uint8_t> bytes) -> std::string
{
    constexpr std::string_view digits = "0123456789abcdef";
    auto result = std::string{};
    result.reserve(bytes.size() * 2U);
    for(const auto byte : bytes)
    {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

[[nodiscard]] auto hex_value(const char value) noexcept
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

[[nodiscard]] auto parse_pack_oid(
    const std::string_view text,
    const core::git_hash_algorithm algorithm)
    -> std::optional<application::git_pack_oid>
{
    const auto byte_count =
        algorithm == core::git_hash_algorithm::sha1
        ? 20U
        : algorithm == core::git_hash_algorithm::sha256 ? 32U : 0U;
    if(text.size() != byte_count * 2U)
    {
        return std::nullopt;
    }
    auto result = application::git_pack_oid{.Algorithm = algorithm};
    for(std::size_t index = 0U; index < byte_count; ++index)
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
    return application::valid(result)
        ? std::optional<application::git_pack_oid>{result}
        : std::nullopt;
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
#elif defined(Q_OS_WIN)
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
#else
    return false;
#endif
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
        && information.st_nlink == 1
        && (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#elif defined(Q_OS_WIN)
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
#else
    return false;
#endif
}

enum class exact_artifact_kind : std::uint8_t
{
    regular_file,
    directory,
};

struct exact_artifact_identity
{
    exact_artifact_kind Kind = exact_artifact_kind::regular_file;
    std::uint64_t Volume = 0U;
    std::uint64_t ObjectHigh = 0U;
    std::uint64_t ObjectLow = 0U;

    auto operator<=>(const exact_artifact_identity&) const = default;
};

#if defined(Q_OS_UNIX)
[[nodiscard]] auto exact_identity(
    const struct stat& information,
    const exact_artifact_kind kind)
    -> std::optional<exact_artifact_identity>
{
    const auto expected_type =
        kind == exact_artifact_kind::regular_file
        ? S_ISREG(information.st_mode)
        : S_ISDIR(information.st_mode);
    if(
        !expected_type || information.st_uid != ::geteuid()
        || information.st_nlink == 0
        || (information.st_mode & (S_IRWXG | S_IRWXO)) != 0
        || (kind == exact_artifact_kind::regular_file
            && information.st_nlink != 1))
    {
        return std::nullopt;
    }
    return exact_artifact_identity{
        .Kind = kind,
        .Volume = static_cast<std::uint64_t>(information.st_dev),
        .ObjectHigh = 0U,
        .ObjectLow = static_cast<std::uint64_t>(information.st_ino),
    };
}

[[nodiscard]] auto exact_identity(
    const int descriptor,
    const exact_artifact_kind kind)
    -> std::optional<exact_artifact_identity>
{
    struct stat information
    {
    };
    return ::fstat(descriptor, &information) == 0
        ? exact_identity(information, kind)
        : std::nullopt;
}
#elif defined(Q_OS_WIN)
[[nodiscard]] auto exact_identity(
    const HANDLE handle,
    const exact_artifact_kind kind)
    -> std::optional<exact_artifact_identity>
{
    FILE_ATTRIBUTE_TAG_INFO tag{};
    BY_HANDLE_FILE_INFORMATION information{};
    if(
        GetFileInformationByHandleEx(
            handle,
            FileAttributeTagInfo,
            &tag,
            sizeof(tag))
                == 0
        || (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
        || GetFileInformationByHandle(handle, &information) == 0
        || information.nNumberOfLinks == 0U
        || (kind == exact_artifact_kind::regular_file
            && information.nNumberOfLinks != 1U))
    {
        return std::nullopt;
    }
    const auto directory =
        (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
    if(
        directory
        != (kind == exact_artifact_kind::directory))
    {
        return std::nullopt;
    }
    return exact_artifact_identity{
        .Kind = kind,
        .Volume =
            static_cast<std::uint64_t>(
                information.dwVolumeSerialNumber),
        .ObjectHigh =
            static_cast<std::uint64_t>(
                information.nFileIndexHigh),
        .ObjectLow =
            static_cast<std::uint64_t>(
                information.nFileIndexLow),
    };
}
#endif

[[nodiscard]] auto exact_identity(
    const std::filesystem::path& path,
    const exact_artifact_kind kind)
    -> std::optional<exact_artifact_identity>
{
#if defined(Q_OS_UNIX)
    struct stat information
    {
    };
    return ::lstat(path.c_str(), &information) == 0
        ? exact_identity(information, kind)
        : std::nullopt;
#elif defined(Q_OS_WIN)
    const auto handle = CreateFileW(
        path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT
            | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    const auto result = exact_identity(handle, kind);
    static_cast<void>(CloseHandle(handle));
    return result;
#else
    static_cast<void>(path);
    static_cast<void>(kind);
    return std::nullopt;
#endif
}

[[nodiscard]] auto missing(const std::filesystem::path& path) -> bool
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    return (
               !error
               && status.type()
                   == std::filesystem::file_type::not_found)
        || error
            == std::make_error_code(
                std::errc::no_such_file_or_directory);
}

[[nodiscard]] auto make_private_directory(
    const std::filesystem::path& path) -> bool
{
    if(private_directory(path))
    {
        return true;
    }
    if(!missing(path) || !private_directory(path.parent_path()))
    {
        return false;
    }
    std::error_code error;
    if(!std::filesystem::create_directory(path, error) || error)
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

[[nodiscard]] auto read_private_file(
    const std::filesystem::path& path,
    const std::size_t maximum_bytes)
    -> std::optional<std::vector<std::uint8_t>>
{
    if(!private_regular_file(path))
    {
        return std::nullopt;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if(error || size > maximum_bytes)
    {
        return std::nullopt;
    }
    auto result = std::vector<std::uint8_t>(
        static_cast<std::size_t>(size));
#if defined(Q_OS_UNIX)
    const auto descriptor =
        ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if(descriptor < 0)
    {
        return std::nullopt;
    }
    std::size_t offset = 0U;
    while(offset < result.size())
    {
        const auto received = ::read(
            descriptor,
            result.data() + offset,
            result.size() - offset);
        if(received < 0 && errno == EINTR)
        {
            continue;
        }
        if(received <= 0)
        {
            static_cast<void>(::close(descriptor));
            return std::nullopt;
        }
        offset += static_cast<std::size_t>(received);
    }
    struct stat information
    {
    };
    const auto valid =
        ::fstat(descriptor, &information) == 0
        && S_ISREG(information.st_mode)
        && information.st_uid == ::geteuid()
        && information.st_nlink == 1
        && information.st_size >= 0
        && static_cast<std::uint64_t>(information.st_size) == size
        && (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
    static_cast<void>(::close(descriptor));
    return valid
        ? std::optional<std::vector<std::uint8_t>>{std::move(result)}
        : std::nullopt;
#elif defined(Q_OS_WIN)
    const auto handle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        0U,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    std::size_t offset = 0U;
    while(offset < result.size())
    {
        const auto requested = static_cast<DWORD>(
            std::min<std::size_t>(
                result.size() - offset,
                std::numeric_limits<DWORD>::max()));
        DWORD received = 0U;
        if(
            ReadFile(
                handle,
                result.data() + offset,
                requested,
                &received,
                nullptr)
                == 0
            || received == 0U)
        {
            static_cast<void>(CloseHandle(handle));
            return std::nullopt;
        }
        offset += received;
    }
    static_cast<void>(CloseHandle(handle));
    return result;
#else
    static_cast<void>(maximum_bytes);
    return std::nullopt;
#endif
}

[[nodiscard]] auto write_new_private_file(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> bytes) -> bool
{
    if(
        path.empty() || !path.is_absolute()
        || !private_directory(path.parent_path()))
    {
        return false;
    }
#if defined(Q_OS_UNIX)
    const auto descriptor = ::open(
        path.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        S_IRUSR | S_IWUSR);
    if(descriptor < 0)
    {
        return false;
    }
    std::size_t offset = 0U;
    while(offset < bytes.size())
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
            static_cast<void>(::close(descriptor));
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    const auto synchronized = ::fsync(descriptor) == 0;
    static_cast<void>(::close(descriptor));
    return synchronized && private_regular_file(path);
#elif defined(Q_OS_WIN)
    const auto handle = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0U,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
            | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    std::size_t offset = 0U;
    while(offset < bytes.size())
    {
        const auto requested = static_cast<DWORD>(
            std::min<std::size_t>(
                bytes.size() - offset,
                std::numeric_limits<DWORD>::max()));
        DWORD written = 0U;
        if(
            WriteFile(
                handle,
                bytes.data() + offset,
                requested,
                &written,
                nullptr)
                == 0
            || written == 0U)
        {
            static_cast<void>(CloseHandle(handle));
            return false;
        }
        offset += written;
    }
    const auto synchronized = FlushFileBuffers(handle) != 0;
    static_cast<void>(CloseHandle(handle));
    return synchronized && private_regular_file(path);
#else
    static_cast<void>(bytes);
    return false;
#endif
}

auto append_u64(
    std::vector<std::uint8_t>& output,
    const std::uint64_t value) -> void
{
    for(unsigned int shift = 56U;; shift -= 8U)
    {
        output.push_back(
            static_cast<std::uint8_t>(value >> shift));
        if(shift == 0U)
        {
            break;
        }
    }
}

[[nodiscard]] auto read_u64(
    const std::span<const std::uint8_t> input,
    const std::size_t offset) -> std::optional<std::uint64_t>
{
    if(offset > input.size() || input.size() - offset < 8U)
    {
        return std::nullopt;
    }
    auto result = std::uint64_t{0U};
    for(std::size_t index = 0U; index < 8U; ++index)
    {
        result = static_cast<std::uint64_t>(
            (result << 8U) | input[offset + index]);
    }
    return result;
}

auto append_identity(
    std::vector<std::uint8_t>& output,
    const exact_artifact_identity& identity) -> void
{
    append_u64(output, identity.Volume);
    append_u64(output, identity.ObjectHigh);
    append_u64(output, identity.ObjectLow);
}

[[nodiscard]] auto read_identity(
    const std::span<const std::uint8_t> input,
    const std::size_t offset,
    const exact_artifact_kind kind)
    -> std::optional<exact_artifact_identity>
{
    const auto volume = read_u64(input, offset);
    const auto high = read_u64(input, offset + 8U);
    const auto low = read_u64(input, offset + 16U);
    if(!volume.has_value() || !high.has_value() || !low.has_value())
    {
        return std::nullopt;
    }
    return exact_artifact_identity{
        .Kind = kind,
        .Volume = *volume,
        .ObjectHigh = *high,
        .ObjectLow = *low,
    };
}

[[nodiscard]] auto artifact_identity(
    const exact_artifact_identity& identity)
    -> std::optional<application::git_artifact_identity>
{
    if(identity.Kind != exact_artifact_kind::regular_file)
    {
        return std::nullopt;
    }
    auto result = application::git_artifact_identity{};
    result.Bytes[0U] = 1U;
    result.Bytes[1U] = 1U;
    result.Bytes[2U] = artifact_identity_scheme;
    const auto write_u64 =
        [&](const std::size_t offset, const std::uint64_t value)
    {
        for(std::size_t index = 0U; index < 8U; ++index)
        {
            result.Bytes[offset + index] =
                static_cast<std::uint8_t>(
                    value >> (56U - index * 8U));
        }
    };
    write_u64(8U, identity.Volume);
    write_u64(16U, identity.ObjectHigh);
    write_u64(24U, identity.ObjectLow);
    return application::valid(result)
        ? std::optional<application::git_artifact_identity>{result}
        : std::nullopt;
}

[[nodiscard]] auto exact_identity(
    const application::git_artifact_identity& identity)
    -> std::optional<exact_artifact_identity>
{
    if(
        !application::valid(identity)
        || identity.Bytes[1U] != 1U
        || identity.Bytes[2U]
            != artifact_identity_scheme
        || std::ranges::any_of(
            std::span{identity.Bytes}.subspan(3U, 5U),
            [](const auto byte)
            {
                return byte != 0U;
            }))
    {
        return std::nullopt;
    }
    const auto bytes = std::span{identity.Bytes};
    const auto volume = read_u64(bytes, 8U);
    const auto high = read_u64(bytes, 16U);
    const auto low = read_u64(bytes, 24U);
    return volume.has_value() && high.has_value()
            && low.has_value()
        ? std::optional<exact_artifact_identity>{
              exact_artifact_identity{
                  .Kind =
                      exact_artifact_kind::regular_file,
                  .Volume = *volume,
                  .ObjectHigh = *high,
                  .ObjectLow = *low,
              }}
        : std::nullopt;
}

[[nodiscard]] auto manifest_authority_bytes(
    const application::git_cache_open_request& request)
    -> std::vector<std::uint8_t>
{
    auto result = std::vector<std::uint8_t>{};
    result.reserve(12U + 1U + 6U * 8U + 3U * 32U);
    result.insert(
        result.end(),
        manifest_magic.begin(),
        manifest_magic.end());
    result.push_back(
        request.ObjectFormat == core::git_hash_algorithm::sha1
        ? 1U
        : 2U);
    append_u64(result, request.Authority.Profile.value());
    append_u64(result, request.Authority.ProfileRevision.value());
    append_u64(result, request.Authority.Host.value());
    result.insert(
        result.end(),
        request.Authority.HostAuthority.bytes().begin(),
        request.Authority.HostAuthority.bytes().end());
    append_u64(result, request.Authority.Account.value());
    append_u64(result, request.Authority.Repository.value());
    result.insert(
        result.end(),
        request.Authority.Cache.bytes().begin(),
        request.Authority.Cache.bytes().end());
    return result;
}

[[nodiscard]] auto manifest_bytes(
    const application::git_cache_open_request& request,
    const exact_artifact_identity& lock_identity)
    -> std::vector<std::uint8_t>
{
    auto result = manifest_authority_bytes(request);
    result.reserve(result.size() + 24U);
    append_identity(result, lock_identity);
    return result;
}

struct deletion_tombstone_record
{
    exact_artifact_identity Repository{
        .Kind = exact_artifact_kind::directory};
    exact_artifact_identity Lock{
        .Kind = exact_artifact_kind::regular_file};
};

[[nodiscard]] auto deletion_tombstone_bytes(
    const application::git_cache_open_request& request,
    const application::git_cache_delete_authorization& authorization,
    const deletion_tombstone_record& record)
    -> std::vector<std::uint8_t>
{
    const auto authority = manifest_authority_bytes(request);
    auto result = std::vector<std::uint8_t>{};
    result.reserve(
        deletion_tombstone_magic.size() + authority.size()
        + authorization.DurableIntentToken.size() + 48U);
    result.insert(
        result.end(),
        deletion_tombstone_magic.begin(),
        deletion_tombstone_magic.end());
    result.insert(result.end(), authority.begin(), authority.end());
    result.insert(
        result.end(),
        authorization.DurableIntentToken.begin(),
        authorization.DurableIntentToken.end());
    append_identity(result, record.Repository);
    append_identity(result, record.Lock);
    return result;
}

[[nodiscard]] auto parse_deletion_tombstone(
    const std::span<const std::uint8_t> bytes,
    const application::git_cache_open_request& request,
    const application::git_cache_delete_authorization& authorization)
    -> std::optional<deletion_tombstone_record>
{
    const auto authority = manifest_authority_bytes(request);
    auto prefix = std::vector<std::uint8_t>{};
    prefix.reserve(
        deletion_tombstone_magic.size() + authority.size()
        + authorization.DurableIntentToken.size());
    prefix.insert(
        prefix.end(),
        deletion_tombstone_magic.begin(),
        deletion_tombstone_magic.end());
    prefix.insert(prefix.end(), authority.begin(), authority.end());
    prefix.insert(
        prefix.end(),
        authorization.DurableIntentToken.begin(),
        authorization.DurableIntentToken.end());
    if(
        bytes.size() != prefix.size() + 48U
        || !std::equal(prefix.begin(), prefix.end(), bytes.begin()))
    {
        return std::nullopt;
    }
    const auto repository = read_identity(
        bytes,
        prefix.size(),
        exact_artifact_kind::directory);
    const auto lock = read_identity(
        bytes,
        prefix.size() + 24U,
        exact_artifact_kind::regular_file);
    return repository.has_value() && lock.has_value()
        ? std::optional<deletion_tombstone_record>{
              deletion_tombstone_record{
                  .Repository = *repository,
                  .Lock = *lock,
              }}
        : std::nullopt;
}

[[nodiscard]] auto repository_config(
    const core::git_hash_algorithm algorithm) -> std::string_view
{
    return algorithm == core::git_hash_algorithm::sha1
        ? std::string_view{
              "[core]\n"
              "\trepositoryformatversion = 0\n"
              "\tbare = true\n"
              "\tlogallrefupdates = false\n"}
        : std::string_view{
              "[core]\n"
              "\trepositoryformatversion = 1\n"
              "\tbare = true\n"
              "\tlogallrefupdates = false\n"
              "[extensions]\n"
              "\tobjectformat = sha256\n"};
}

[[nodiscard]] auto read_exact_private_text(
    const std::filesystem::path& path,
    const std::string_view expected) -> bool
{
    const auto actual = read_private_file(path, expected.size());
    return actual.has_value() && actual->size() == expected.size()
        && std::equal(
            actual->begin(),
            actual->end(),
            reinterpret_cast<const std::uint8_t*>(
                expected.data()));
}

class cache_lock final
{
public:
    cache_lock() = default;
    cache_lock(const cache_lock&) = delete;
    auto operator=(const cache_lock&) -> cache_lock& = delete;

    ~cache_lock()
    {
#if defined(Q_OS_UNIX)
        if(Descriptor >= 0)
        {
            static_cast<void>(::flock(Descriptor, LOCK_UN));
            static_cast<void>(::close(Descriptor));
        }
#elif defined(Q_OS_WIN)
        if(Handle != INVALID_HANDLE_VALUE)
        {
            static_cast<void>(CloseHandle(Handle));
        }
#endif
    }

    [[nodiscard]] auto acquire(
        const std::filesystem::path& path) -> bool
    {
        if(!private_directory(path.parent_path()))
        {
            return false;
        }
#if defined(Q_OS_UNIX)
        Descriptor = ::open(
            path.c_str(),
            O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR);
        if(Descriptor < 0)
        {
            return false;
        }
        struct stat information
        {
        };
        if(
            ::fstat(Descriptor, &information) != 0
            || !S_ISREG(information.st_mode)
            || information.st_uid != ::geteuid()
            || information.st_nlink != 1
            || (information.st_mode & (S_IRWXG | S_IRWXO)) != 0
            || ::flock(Descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            static_cast<void>(::close(Descriptor));
            Descriptor = -1;
            return false;
        }
        Identity = exact_identity(
            information,
            exact_artifact_kind::regular_file);
        if(!Identity.has_value())
        {
            static_cast<void>(::flock(Descriptor, LOCK_UN));
            static_cast<void>(::close(Descriptor));
            Descriptor = -1;
            return false;
        }
        return true;
#elif defined(Q_OS_WIN)
        Handle = CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0U,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);
        if(Handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        FILE_ATTRIBUTE_TAG_INFO information{};
        if(
            GetFileInformationByHandleEx(
                Handle,
                FileAttributeTagInfo,
                &information,
                sizeof(information))
                == 0
            || (information.FileAttributes
                & FILE_ATTRIBUTE_REPARSE_POINT)
                != 0U
            || (information.FileAttributes
                & FILE_ATTRIBUTE_DIRECTORY)
                != 0U)
        {
            static_cast<void>(CloseHandle(Handle));
            Handle = INVALID_HANDLE_VALUE;
            return false;
        }
        Identity = exact_identity(
            Handle,
            exact_artifact_kind::regular_file);
        if(!Identity.has_value())
        {
            static_cast<void>(CloseHandle(Handle));
            Handle = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
#else
        static_cast<void>(path);
        return false;
#endif
    }

    [[nodiscard]] auto identity() const noexcept
        -> std::optional<exact_artifact_identity>
    {
        return Identity;
    }

    [[nodiscard]] auto still_names(
        const std::filesystem::path& path) const -> bool
    {
        return Identity.has_value()
            && exact_identity(
                   path,
                   exact_artifact_kind::regular_file)
                == Identity
            && private_regular_file(path);
    }

private:
    std::optional<exact_artifact_identity> Identity;
#if defined(Q_OS_UNIX)
    int Descriptor = -1;
#elif defined(Q_OS_WIN)
    HANDLE Handle = INVALID_HANDLE_VALUE;
#endif
};

[[nodiscard]] auto process_algorithm(
    const core::git_hash_algorithm algorithm)
    -> std::optional<application::git_hash_algorithm>
{
    if(algorithm == core::git_hash_algorithm::sha1)
    {
        return application::git_hash_algorithm::sha1;
    }
    if(algorithm == core::git_hash_algorithm::sha256)
    {
        return application::git_hash_algorithm::sha256;
    }
    return std::nullopt;
}

[[nodiscard]] auto process_operation(
    const core::epoch_operation_id& operation)
    -> application::git_operation_identity
{
    return {.Bytes = operation.bytes()};
}

[[nodiscard]] auto process_scope(
    const core::pull_request_authority_scope_id& scope)
    -> application::git_pull_request_scope_identity
{
    return {.Bytes = scope.bytes()};
}

[[nodiscard]] auto process_pack(
    const application::git_pack_oid& pack)
    -> std::optional<application::git_pack_id>
{
    const auto algorithm = process_algorithm(pack.Algorithm);
    return algorithm.has_value()
        ? std::optional<application::git_pack_id>{
              application::git_pack_id{
                  .Algorithm = *algorithm,
                  .Digest = pack.Digest}}
        : std::nullopt;
}

[[nodiscard]] auto process_commit(
    const core::git_commit_oid& commit)
    -> std::optional<application::git_commit_id>
{
    const auto algorithm = process_algorithm(commit.algorithm());
    return algorithm.has_value() && commit.valid()
        ? std::optional<application::git_commit_id>{
              application::git_commit_id{
                  .Algorithm = *algorithm,
                  .Digest = commit.bytes()}}
        : std::nullopt;
}

[[nodiscard]] auto core_commit(
    const application::git_commit_id& commit)
    -> core::git_commit_oid
{
    const auto algorithm =
        commit.Algorithm == application::git_hash_algorithm::sha1
        ? core::git_hash_algorithm::sha1
        : core::git_hash_algorithm::sha256;
    return core::git_commit_oid{algorithm, commit.Digest};
}

[[nodiscard]] auto core_tree(
    const application::git_tree_id& tree)
    -> core::git_tree_oid
{
    const auto algorithm =
        tree.Algorithm == application::git_hash_algorithm::sha1
        ? core::git_hash_algorithm::sha1
        : core::git_hash_algorithm::sha256;
    return core::git_tree_oid{algorithm, tree.Digest};
}

[[nodiscard]] auto staging_ref(
    const core::epoch_operation_id& operation,
    const application::git_private_ref_endpoint endpoint)
    -> application::git_private_ref
{
    return application::git_staging_ref{
        .Operation = process_operation(operation),
        .Endpoint = endpoint,
    };
}

[[nodiscard]] auto retained_ref(
    const core::pull_request_authority_scope_id& scope,
    const core::epoch_ordinal ordinal,
    const application::git_private_ref_endpoint endpoint)
    -> application::git_private_ref
{
    return application::git_epoch_ref{
        .Scope = process_scope(scope),
        .Ordinal = static_cast<std::uint64_t>(ordinal.value()),
        .Endpoint = endpoint,
    };
}

struct inventory_snapshot
{
    std::vector<application::git_pack_inventory_entry> Packs;
    std::uint64_t RepositoryBytes = 0U;
    std::uint32_t PrivateRefs = 0U;
};

[[nodiscard]] auto same_inventory(
    const inventory_snapshot& left,
    const inventory_snapshot& right) noexcept -> bool
{
    return left.Packs == right.Packs
        && left.RepositoryBytes == right.RepositoryBytes
        && left.PrivateRefs == right.PrivateRefs;
}

class cache_state;

struct receipt_record
{
    application::completed_git_pack_receipt Receipt;
    std::filesystem::path Spool;
    exact_artifact_identity SpoolIdentity;
    std::uint64_t MaximumIndexBytes = 0U;
};

struct spool_record
{
    core::epoch_operation_id Operation;
    std::filesystem::path Receiving;
    std::filesystem::path Completed;
    exact_artifact_identity Identity;
};

struct observation_record
{
    application::git_preimport_observation Observation;
    application::completed_git_pack_receipt Receipt;
    std::uint64_t MaximumIndexBytes = 0U;
};

class cache_state final
    : public std::enable_shared_from_this<cache_state>
{
public:
    cache_state(
        application::git_cache_open_request request,
        std::filesystem::path storage_root,
        std::filesystem::path repository,
        std::optional<exact_artifact_identity> repository_identity,
        std::filesystem::path lock_path,
        exact_artifact_identity lock_identity,
        std::unique_ptr<cache_lock> lock,
        std::unique_ptr<application::git_process_port> process)
        : Request{std::move(request)},
          StorageRoot{std::move(storage_root)},
          Repository{std::move(repository)},
          RepositoryIdentity{repository_identity},
          LockPath{std::move(lock_path)},
          LockIdentity{lock_identity},
          Lock{std::move(lock)},
          Process{std::move(process)}
    {
    }

    [[nodiscard]] auto bind_repository(
        std::filesystem::path repository,
        const exact_artifact_identity& identity) -> bool
    {
        if(
            identity.Kind != exact_artifact_kind::directory
            || !private_directory(repository)
            || exact_identity(repository, identity.Kind) != identity)
        {
            return false;
        }
        Repository = std::move(repository);
        RepositoryIdentity = identity;
        return true;
    }

    [[nodiscard]] auto repository_root_still_names() const -> bool
    {
        return RepositoryIdentity.has_value()
            && RepositoryIdentity->Kind
                == exact_artifact_kind::directory
            && exact_identity(
                   Repository,
                   exact_artifact_kind::directory)
                == RepositoryIdentity
            && private_directory(Repository);
    }

    [[nodiscard]] auto active_boundary_still_owned() const -> bool
    {
        return repository_root_still_names()
            && exact_identity(
                   LockPath,
                   exact_artifact_kind::regular_file)
                == LockIdentity
            && private_regular_file(LockPath);
    }

    [[nodiscard]] auto validate_topology(
        const std::stop_token stop = {}) const -> bool
    {
        if(
            stop.stop_requested()
            || !active_boundary_still_owned()
            || !private_directory(StorageRoot)
            || !private_directory(StorageRoot / "repositories")
            || !private_directory(
                StorageRoot / "repositories" / "v1")
            || !private_directory(StorageRoot / "incoming")
            || !private_directory(StorageRoot / "incoming" / "v1")
            || !private_directory(StorageRoot / "locks")
            || !private_directory(StorageRoot / "locks" / "v1")
            || !private_directory(StorageRoot / "templates")
            || !private_directory(StorageRoot / "hooks")
            || !private_directory(Repository)
            || !private_directory(Repository / "objects")
            || !private_directory(Repository / "refs")
            || !read_exact_private_text(
                Repository / "config",
                repository_config(Request.ObjectFormat))
            || !read_exact_private_text(
                Repository / "HEAD",
                "ref: refs/heads/main\n"))
        {
            return false;
        }
        const auto expected = manifest_bytes(Request, LockIdentity);
        const auto manifest = read_private_file(
            Repository / manifest_name,
            expected.size());
        if(!manifest.has_value() || *manifest != expected)
        {
            return false;
        }
        const std::array forbidden{
            Repository / "objects" / "info" / "alternates",
            Repository / "objects" / "info" / "http-alternates",
            Repository / "info" / "grafts",
            Repository / "refs" / "replace",
            Repository / "shallow",
            Repository / "worktrees",
            Repository / "packed-refs",
        };
        return std::all_of(
                   forbidden.begin(),
                   forbidden.end(),
                   missing)
            && active_boundary_still_owned();
    }

    [[nodiscard]] auto inventory(
        const std::stop_token stop = {},
        const bool enforce_limits = true) const
        -> application::git_cache_result<inventory_snapshot>
    {
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(!active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto repository_identity = *RepositoryIdentity;
        auto snapshot = inventory_snapshot{};
        std::error_code error;
        auto entry_count = std::size_t{0U};
        for(
            auto iterator = std::filesystem::recursive_directory_iterator{
                Repository,
                std::filesystem::directory_options::none,
                error};
            iterator
            != std::filesystem::recursive_directory_iterator{};
            iterator.increment(error))
        {
            if(stop.stop_requested())
            {
                return std::unexpected{
                    application::git_cache_failure::cancelled};
            }
            if(error || ++entry_count > maximum_repository_entries)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto path = iterator->path();
            const auto status =
                std::filesystem::symlink_status(path, error);
            if(error || std::filesystem::is_symlink(status))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            if(
                status.type()
                == std::filesystem::file_type::directory)
            {
                const auto identity = exact_identity(
                    path,
                    exact_artifact_kind::directory);
                if(
                    !identity.has_value()
                    || identity->Volume
                        != repository_identity.Volume
                    || !private_directory(path))
                {
                    return std::unexpected{
                        application::git_cache_failure::
                            invalid_repository};
                }
                continue;
            }
            if(
                status.type()
                    != std::filesystem::file_type::regular
                || !private_regular_file(path))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto identity = exact_identity(
                path,
                exact_artifact_kind::regular_file);
            if(
                !identity.has_value()
                || identity->Volume
                    != repository_identity.Volume)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto size = std::filesystem::file_size(path, error);
            if(
                error
                || size
                    > std::numeric_limits<std::uint64_t>::max()
                        - snapshot.RepositoryBytes)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        quota_exceeded};
            }
            snapshot.RepositoryBytes += size;
            const auto relative =
                path.lexically_relative(Repository).generic_string();
            if(relative.starts_with("refs/betterpullreview/"))
            {
                if(
                    snapshot.PrivateRefs
                    == std::numeric_limits<std::uint32_t>::max())
                {
                    return std::unexpected{
                        application::git_cache_failure::
                            quota_exceeded};
                }
                ++snapshot.PrivateRefs;
            }
        }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
        std::error_code recursive_scan_injection_error;
        if(
            !error
            && std::filesystem::exists(
                StorageRoot
                    / "inject-cache-recursive-scan-terminal-error",
                recursive_scan_injection_error)
            && !recursive_scan_injection_error)
        {
            error = std::make_error_code(std::errc::io_error);
        }
        else if(recursive_scan_injection_error)
        {
            error = recursive_scan_injection_error;
        }
#endif
        if(error)
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        if(
            enforce_limits
            && (snapshot.RepositoryBytes
                    > Request.Limits.RepositoryBytes
                || snapshot.PrivateRefs
                    > Request.Limits.PrivateRefCount))
        {
            return std::unexpected{
                application::git_cache_failure::quota_exceeded};
        }

        const auto pack_directory =
            Repository / "objects" / "pack";
        if(missing(pack_directory))
        {
            return active_boundary_still_owned()
                ? application::git_cache_result<
                      inventory_snapshot>{std::move(snapshot)}
                : application::git_cache_result<
                      inventory_snapshot>{
                      std::unexpected{
                          application::git_cache_failure::
                              invalid_repository}};
        }
        if(!private_directory(pack_directory))
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        struct pair_sizes
        {
            application::git_pack_oid Pack;
            std::optional<std::uint64_t> PackBytes;
            std::optional<std::uint64_t> IndexBytes;
        };
        auto pairs = std::map<std::string, pair_sizes>{};
        auto pack_iterator = std::filesystem::directory_iterator{
            pack_directory,
            std::filesystem::directory_options::none,
            error};
        const auto pack_end =
            std::filesystem::directory_iterator{};
        while(!error && pack_iterator != pack_end)
        {
            const auto entry = *pack_iterator;
            if(stop.stop_requested())
            {
                return std::unexpected{
                    application::git_cache_failure::cancelled};
            }
            if(error || !private_regular_file(entry.path()))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto name = entry.path().filename().string();
            const auto digest_characters =
                Request.ObjectFormat
                    == core::git_hash_algorithm::sha1
                ? 40U
                : 64U;
            const auto suffix =
                name.ends_with(".pack")
                ? std::string_view{".pack"}
                : name.ends_with(".idx")
                    ? std::string_view{".idx"}
                    : std::string_view{};
            if(
                suffix.empty()
                || name.size()
                    != 5U + digest_characters + suffix.size()
                || !name.starts_with("pack-"))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto digest =
                std::string_view{name}.substr(5U, digest_characters);
            const auto parsed =
                parse_pack_oid(digest, Request.ObjectFormat);
            if(!parsed.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto size =
                std::filesystem::file_size(entry.path(), error);
            if(error || size == 0U)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            auto& pair = pairs[std::string{digest}];
            pair.Pack = *parsed;
            auto& destination =
                suffix == ".pack" ? pair.PackBytes : pair.IndexBytes;
            if(destination.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            destination = size;
            pack_iterator.increment(error);
        }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
        std::error_code scan_injection_error;
        if(
            !error
            && std::filesystem::exists(
                StorageRoot
                    / "inject-cache-pack-scan-terminal-error",
                scan_injection_error)
            && !scan_injection_error)
        {
            error = std::make_error_code(std::errc::io_error);
        }
        else if(scan_injection_error)
        {
            error = scan_injection_error;
        }
#endif
        if(error)
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        if(enforce_limits && pairs.size() > Request.Limits.PackCount)
        {
            return std::unexpected{
                application::git_cache_failure::quota_exceeded};
        }
        snapshot.Packs.reserve(pairs.size());
        for(const auto& [digest, pair] : pairs)
        {
            static_cast<void>(digest);
            if(
                !pair.PackBytes.has_value()
                || !pair.IndexBytes.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            snapshot.Packs.push_back({
                .Pack = pair.Pack,
                .PackBytes = *pair.PackBytes,
                .IndexBytes = *pair.IndexBytes,
            });
        }
        return active_boundary_still_owned()
            ? application::git_cache_result<
                  inventory_snapshot>{std::move(snapshot)}
            : application::git_cache_result<
                  inventory_snapshot>{
                  std::unexpected{
                      application::git_cache_failure::
                          invalid_repository}};
    }

    [[nodiscard]] auto validated_inventory(
        const std::stop_token stop = {},
        const bool enforce_limits = true) const
        -> application::git_cache_result<inventory_snapshot>
    {
        if(!validate_topology(stop))
        {
            return std::unexpected{
                stop.stop_requested()
                    ? application::git_cache_failure::cancelled
                    : application::git_cache_failure::
                          invalid_repository};
        }
        return inventory(stop, enforce_limits);
    }

    [[nodiscard]] auto process_failure(
        const application::git_process_failure failure)
        -> application::git_cache_failure
    {
        if(failure == application::git_process_failure::reap_failure)
        {
            retain_after_reap_failure();
            return application::git_cache_failure::repair_required;
        }
        if(failure == application::git_process_failure::cancelled)
        {
            return application::git_cache_failure::cancelled;
        }
        if(
            failure
            == application::git_process_failure::invalid_repository)
        {
            return application::git_cache_failure::invalid_repository;
        }
        return application::git_cache_failure::process_failure;
    }

    auto retain_after_reap_failure() -> void
    {
        if(!Retained.exchange(true))
        {
            static_cast<void>(
                new std::shared_ptr<cache_state>{
                    shared_from_this()});
        }
    }

    [[nodiscard]] auto new_token()
        -> std::array<std::uint8_t, 32U>
    {
        auto token = std::array<std::uint8_t, 32U>{};
        auto generator = QRandomGenerator::securelySeeded();
        do
        {
            for(std::size_t index = 0U; index < token.size(); index += 4U)
            {
                const auto value = generator.generate();
                token[index] = static_cast<std::uint8_t>(value >> 24U);
                token[index + 1U] =
                    static_cast<std::uint8_t>(value >> 16U);
                token[index + 2U] =
                    static_cast<std::uint8_t>(value >> 8U);
                token[index + 3U] =
                    static_cast<std::uint8_t>(value);
            }
        } while(!application::valid_cache_token(token));
        return token;
    }

    auto register_receipt(receipt_record record) -> bool
    {
        std::lock_guard guard{RecordsMutex};
        if(
            LifecycleExhausted
            || !application::agrees(record.Receipt)
            || exact_identity(
                   record.Receipt.Spool.Identity)
                != record.SpoolIdentity
            || std::ranges::find(
                AbortedOperations,
                record.Receipt.Operation)
            != AbortedOperations.end())
        {
            return false;
        }
        const auto spool = std::ranges::find_if(
            Spools,
            [&](const auto& existing)
            {
                return existing.Operation
                        == record.Receipt.Operation
                    && existing.Completed == record.Spool
                    && existing.Identity == record.SpoolIdentity;
            });
        const auto duplicate = std::ranges::any_of(
            Receipts,
            [&](const auto& existing)
            {
                return existing.Receipt.Operation
                    == record.Receipt.Operation;
            });
        if(
            spool == Spools.end() || duplicate
            || Receipts.size() >= Request.Limits.PackCount)
        {
            return false;
        }
        Receipts.push_back(std::move(record));
        return true;
    }

    auto register_spool(spool_record record) -> bool
    {
        std::lock_guard guard{RecordsMutex};
        if(
            Spools.size()
                >= application::maximum_git_cache_pack_count
            || LifecycleExhausted
            || std::ranges::find(
                   AbortedOperations,
                   record.Operation)
                != AbortedOperations.end()
            || std::ranges::any_of(
                Spools,
                [&](const auto& existing)
                {
                    return existing.Operation == record.Operation;
                }))
        {
            return false;
        }
        Spools.push_back(std::move(record));
        return true;
    }

    [[nodiscard]] auto operation_aborted(
        const core::epoch_operation_id& operation)
        -> bool
    {
        std::lock_guard guard{RecordsMutex};
        return LifecycleExhausted
            || std::ranges::find(AbortedOperations, operation)
            != AbortedOperations.end();
    }

    [[nodiscard]] auto mark_operation_aborted(
        const core::epoch_operation_id& operation) -> bool
    {
        std::lock_guard guard{RecordsMutex};
        if(
            std::ranges::find(AbortedOperations, operation)
            != AbortedOperations.end())
        {
            return true;
        }
        if(
            AbortedOperations.size()
            >= application::maximum_git_cache_pack_count)
        {
            LifecycleExhausted = true;
            return true;
        }
        AbortedOperations.push_back(operation);
        return true;
    }

    [[nodiscard]] auto find_spool(
        const core::epoch_operation_id& operation)
        -> std::optional<spool_record>
    {
        std::lock_guard guard{RecordsMutex};
        const auto found = std::ranges::find_if(
            Spools,
            [&](const auto& record)
            {
                return record.Operation == operation;
            });
        return found == Spools.end()
            ? std::nullopt
            : std::optional<spool_record>{*found};
    }

    [[nodiscard]] auto find_receipt(
        const application::completed_git_pack_receipt& receipt)
        -> std::optional<receipt_record>
    {
        std::lock_guard guard{RecordsMutex};
        const auto found = std::ranges::find_if(
            Receipts,
            [&](const auto& record)
            {
                return record.Receipt == receipt;
            });
        return found == Receipts.end()
            ? std::nullopt
            : std::optional<receipt_record>{*found};
    }

    auto register_observation(observation_record record) -> bool
    {
        std::lock_guard guard{RecordsMutex};
        const auto found = std::ranges::find_if(
            Observations,
            [&](const auto& existing)
            {
                return existing.Observation.Operation
                    == record.Observation.Operation;
            });
        if(found != Observations.end())
        {
            *found = std::move(record);
            return true;
        }
        if(Observations.size() >= Request.Limits.PackCount)
        {
            return false;
        }
        Observations.push_back(std::move(record));
        return true;
    }

    [[nodiscard]] auto find_observation(
        const application::git_preimport_observation& observation,
        const application::completed_git_pack_receipt& receipt)
        -> std::optional<observation_record>
    {
        std::lock_guard guard{RecordsMutex};
        const auto found = std::ranges::find_if(
            Observations,
            [&](const auto& record)
            {
                return record.Observation == observation
                    && record.Receipt == receipt;
            });
        return found == Observations.end()
            ? std::nullopt
            : std::optional<observation_record>{*found};
    }

    auto forget_operation(
        const core::epoch_operation_id& operation) -> void
    {
        std::lock_guard guard{RecordsMutex};
        std::erase_if(
            Receipts,
            [&](const auto& record)
            {
                return record.Receipt.Operation == operation;
            });
        std::erase_if(
            Observations,
            [&](const auto& record)
            {
                return record.Observation.Operation == operation;
            });
        std::erase_if(
            Spools,
            [&](const auto& record)
            {
                return record.Operation == operation;
            });
    }

    application::git_cache_open_request Request;
    std::filesystem::path StorageRoot;
    std::filesystem::path Repository;
    std::optional<exact_artifact_identity> RepositoryIdentity;
    std::filesystem::path LockPath;
    exact_artifact_identity LockIdentity;
    std::unique_ptr<cache_lock> Lock;
    std::unique_ptr<application::git_process_port> Process;
    std::recursive_mutex OperationMutex;

private:
    std::mutex RecordsMutex;
    std::vector<spool_record> Spools;
    std::vector<core::epoch_operation_id> AbortedOperations;
    bool LifecycleExhausted = false;
    std::vector<receipt_record> Receipts;
    std::vector<observation_record> Observations;
    std::atomic_bool Retained = false;
};

class private_pack_file final
{
public:
    private_pack_file() = default;
    private_pack_file(const private_pack_file&) = delete;
    auto operator=(const private_pack_file&)
        -> private_pack_file& = delete;

    ~private_pack_file()
    {
        close();
    }

    [[nodiscard]] auto create(
        const std::filesystem::path& path) -> bool
    {
        Path = path;
#if defined(Q_OS_UNIX)
        Descriptor = ::open(
            path.c_str(),
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR);
        return Descriptor >= 0;
#elif defined(Q_OS_WIN)
        Handle = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0U,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
                | FILE_FLAG_WRITE_THROUGH,
            nullptr);
        return Handle != INVALID_HANDLE_VALUE;
#else
        return false;
#endif
    }

    [[nodiscard]] auto write(
        const std::span<const std::byte> bytes) -> bool
    {
        auto offset = std::size_t{0U};
        while(offset < bytes.size())
        {
#if defined(Q_OS_UNIX)
            const auto written = ::write(
                Descriptor,
                bytes.data() + offset,
                bytes.size() - offset);
            if(written < 0 && errno == EINTR)
            {
                continue;
            }
            if(written <= 0)
            {
                return false;
            }
            offset += static_cast<std::size_t>(written);
#elif defined(Q_OS_WIN)
            const auto requested = static_cast<DWORD>(
                std::min<std::size_t>(
                    bytes.size() - offset,
                    std::numeric_limits<DWORD>::max()));
            DWORD written = 0U;
            if(
                WriteFile(
                    Handle,
                    bytes.data() + offset,
                    requested,
                    &written,
                    nullptr)
                    == 0
                || written == 0U)
            {
                return false;
            }
            offset += written;
#else
            static_cast<void>(bytes);
            return false;
#endif
        }
        return true;
    }

    [[nodiscard]] auto synchronize_and_close() -> bool
    {
#if defined(Q_OS_UNIX)
        if(Descriptor < 0)
        {
            return false;
        }
        const auto synchronized = ::fsync(Descriptor) == 0;
        const auto closed = ::close(Descriptor) == 0;
        Descriptor = -1;
        return synchronized && closed;
#elif defined(Q_OS_WIN)
        if(Handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        const auto synchronized = FlushFileBuffers(Handle) != 0;
        const auto closed = CloseHandle(Handle) != 0;
        Handle = INVALID_HANDLE_VALUE;
        return synchronized && closed;
#else
        return false;
#endif
    }

    [[nodiscard]] auto identity() const
        -> std::optional<exact_artifact_identity>
    {
#if defined(Q_OS_UNIX)
        return Descriptor >= 0
            ? exact_identity(
                  Descriptor,
                  exact_artifact_kind::regular_file)
            : std::nullopt;
#elif defined(Q_OS_WIN)
        return Handle != INVALID_HANDLE_VALUE
            ? exact_identity(
                  Handle,
                  exact_artifact_kind::regular_file)
            : std::nullopt;
#else
        return std::nullopt;
#endif
    }

    auto close() noexcept -> void
    {
#if defined(Q_OS_UNIX)
        if(Descriptor >= 0)
        {
            static_cast<void>(::close(Descriptor));
            Descriptor = -1;
        }
#elif defined(Q_OS_WIN)
        if(Handle != INVALID_HANDLE_VALUE)
        {
            static_cast<void>(CloseHandle(Handle));
            Handle = INVALID_HANDLE_VALUE;
        }
#endif
    }

private:
    std::filesystem::path Path;
#if defined(Q_OS_UNIX)
    int Descriptor = -1;
#elif defined(Q_OS_WIN)
    HANDLE Handle = INVALID_HANDLE_VALUE;
#endif
};

[[nodiscard]] auto remove_exact_private_file(
    const std::filesystem::path& path,
    const exact_artifact_identity& expected) -> bool;
[[nodiscard]] auto synchronize_directory(
    const std::filesystem::path& path) -> bool;
[[nodiscard]] auto rename_no_replace(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::error_code& error) noexcept -> bool;

struct spool_publish_result
{
    bool Success = false;
    bool CompletedOwned = false;
};

[[nodiscard]] auto publish_spool(
    const std::filesystem::path& receiving,
    const std::filesystem::path& completed) -> spool_publish_result
{
    if(!missing(completed) || !private_regular_file(receiving))
    {
        return {};
    }
#if defined(Q_OS_UNIX)
    std::error_code rename_error;
    if(!rename_no_replace(receiving, completed, rename_error))
    {
        return {};
    }
    const auto directory = ::open(
        completed.parent_path().c_str(),
        O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if(directory < 0)
    {
        return {
            .Success = false,
            .CompletedOwned = true,
        };
    }
    const auto synchronized = ::fsync(directory) == 0;
    static_cast<void>(::close(directory));
    return {
        .Success =
            synchronized && private_regular_file(completed),
        .CompletedOwned = true,
    };
#elif defined(Q_OS_WIN)
    if(
        MoveFileExW(
            receiving.c_str(),
            completed.c_str(),
            MOVEFILE_WRITE_THROUGH)
        == 0)
    {
        return {};
    }
    return {
        .Success = private_regular_file(completed),
        .CompletedOwned = true,
    };
#else
    static_cast<void>(receiving);
    static_cast<void>(completed);
    return {};
#endif
}

class pack_sink final : public application::git_pack_stream_sink
{
public:
    pack_sink(
        std::shared_ptr<cache_state> state,
        core::epoch_operation_id operation,
        std::filesystem::path receiving,
        std::filesystem::path completed,
        const std::uint64_t maximum_bytes,
        const core::git_hash_algorithm algorithm)
        : State{std::move(state)},
          Operation{operation},
          Receiving{std::move(receiving)},
          Completed{std::move(completed)},
          MaximumBytes{maximum_bytes},
          Algorithm{algorithm},
          DigestBytes{
              algorithm == core::git_hash_algorithm::sha1 ? 20U : 32U},
          Hash{
              algorithm == core::git_hash_algorithm::sha1
                  ? QCryptographicHash::Sha1
                  : QCryptographicHash::Sha256}
    {
    }

    ~pack_sink() override
    {
        abort();
    }

    [[nodiscard]] auto initialize() -> bool
    {
        Tail.reserve(DigestBytes);
        Header.reserve(12U);
        if(!File.create(Receiving))
        {
            return false;
        }
        ReceivingIdentity = File.identity();
        Registered = ReceivingIdentity.has_value()
            && State->register_spool({
                .Operation = Operation,
                .Receiving = Receiving,
                .Completed = Completed,
                .Identity = *ReceivingIdentity,
            });
        return Registered;
    }

    [[nodiscard]] auto append(
        const std::span<const std::byte> bytes,
        const std::stop_token stop)
        -> application::git_pack_stream_result<void> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(
            Finished || Aborted
            || State->operation_aborted(Operation)
            || bytes.empty())
        {
            return std::unexpected{
                application::git_pack_stream_failure::
                    invalid_state};
        }
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_pack_stream_failure::cancelled};
        }
        if(
            Bytes > MaximumBytes
            || bytes.size() > MaximumBytes - Bytes)
        {
            return std::unexpected{
                application::git_pack_stream_failure::input_limit};
        }
        const auto header_needed =
            std::min<std::size_t>(
                12U - Header.size(),
                bytes.size());
        for(std::size_t index = 0U; index < header_needed; ++index)
        {
            Header.push_back(
                static_cast<std::uint8_t>(bytes[index]));
        }
        if(!File.write(bytes))
        {
            return std::unexpected{
                application::git_pack_stream_failure::storage};
        }

        const auto* begin = reinterpret_cast<const char*>(bytes.data());
        const auto total = Tail.size() + bytes.size();
        if(total <= DigestBytes)
        {
            Tail.insert(Tail.end(), bytes.begin(), bytes.end());
        }
        else
        {
            const auto hash_bytes = total - DigestBytes;
            const auto from_tail =
                std::min<std::size_t>(hash_bytes, Tail.size());
            if(from_tail != 0U)
            {
                Hash.addData(QByteArrayView{
                    reinterpret_cast<const char*>(Tail.data()),
                    static_cast<qsizetype>(from_tail)});
            }
            const auto from_input = hash_bytes - from_tail;
            if(from_input != 0U)
            {
                Hash.addData(QByteArrayView{
                    begin,
                    static_cast<qsizetype>(from_input)});
            }
            auto replacement = std::vector<std::byte>{};
            replacement.reserve(DigestBytes);
            if(from_tail < Tail.size())
            {
                replacement.insert(
                    replacement.end(),
                    Tail.begin()
                        + static_cast<std::ptrdiff_t>(from_tail),
                    Tail.end());
            }
            replacement.insert(
                replacement.end(),
                bytes.begin()
                    + static_cast<std::ptrdiff_t>(from_input),
                bytes.end());
            Tail = std::move(replacement);
        }
        Bytes += bytes.size();
        return {};
    }

    [[nodiscard]] auto finish(const std::stop_token stop)
        -> application::git_pack_stream_result<
            application::completed_git_pack_receipt> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(
            Finished || Aborted
            || State->operation_aborted(Operation))
        {
            return std::unexpected{
                application::git_pack_stream_failure::
                    invalid_state};
        }
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_pack_stream_failure::cancelled};
        }
        if(
            Bytes <= 12U + DigestBytes || Header.size() != 12U
            || Header[0] != 0x50U || Header[1] != 0x41U
            || Header[2] != 0x43U || Header[3] != 0x4bU
            || Header[4] != 0U || Header[5] != 0U
            || Header[6] != 0U
            || (Header[7] != 2U && Header[7] != 3U)
            || Tail.size() != DigestBytes)
        {
            return std::unexpected{
                application::git_pack_stream_failure::
                    malformed_pack};
        }
        const auto observed_bytes = Hash.result();
        if(
            static_cast<std::size_t>(observed_bytes.size())
                != DigestBytes
            || !std::equal(
                Tail.begin(),
                Tail.end(),
                reinterpret_cast<const std::byte*>(
                    observed_bytes.constData())))
        {
            return std::unexpected{
                application::git_pack_stream_failure::
                    malformed_pack};
        }
        if(!File.synchronize_and_close())
        {
            return std::unexpected{
                application::git_pack_stream_failure::storage};
        }
        const auto maximum_index =
            maximum_index_bytes(Algorithm, Header);
        if(!maximum_index.has_value())
        {
            return std::unexpected{
                application::git_pack_stream_failure::input_limit};
        }
        const auto publication =
            publish_spool(Receiving, Completed);
        CompletedOwned = publication.CompletedOwned;
        if(
            !publication.Success || !ReceivingIdentity.has_value()
            || exact_identity(
                   Completed,
                   exact_artifact_kind::regular_file)
                != ReceivingIdentity)
        {
            return std::unexpected{
                application::git_pack_stream_failure::storage};
        }
        auto pack = application::git_pack_oid{
            .Algorithm = Algorithm};
        std::transform(
            Tail.begin(),
            Tail.end(),
            pack.Digest.begin(),
            [](const std::byte value)
            {
                return static_cast<std::uint8_t>(value);
            });
        const auto durable_spool_identity =
            artifact_identity(*ReceivingIdentity);
        if(!durable_spool_identity.has_value())
        {
            return std::unexpected{
                application::git_pack_stream_failure::storage};
        }
        auto receipt = application::completed_git_pack_receipt{
            .Operation = Operation,
            .ExpectedPack = pack,
            .ObservedPack = pack,
            .Spool =
                {
                    .Role =
                        application::git_artifact_role::
                            completed_spool,
                    .Identity = *durable_spool_identity,
                    .Bytes = Bytes,
                },
            .OpaqueToken = State->new_token(),
        };
        if(
            !State->register_receipt({
                .Receipt = receipt,
                .Spool = Completed,
                .SpoolIdentity = *ReceivingIdentity,
                .MaximumIndexBytes = *maximum_index,
            }))
        {
            return std::unexpected{
                application::git_pack_stream_failure::
                    invalid_state};
        }
        Finished = true;
        CompletedOwned = false;
        return receipt;
    }

    auto abort() noexcept -> void override
    {
        try
        {
            std::lock_guard operation_guard{
                State->OperationMutex};
            if(Finished)
            {
                return;
            }
            if(!Aborted)
            {
                if(Registered)
                {
                    static_cast<void>(
                        State->mark_operation_aborted(Operation));
                }
                File.close();
                Aborted = true;
            }
            auto cleaned = missing(Receiving);
            if(ReceivingIdentity.has_value() && !cleaned)
            {
                cleaned = remove_exact_private_file(
                    Receiving,
                    *ReceivingIdentity);
            }
            if(CompletedOwned)
            {
                auto completed_cleaned = missing(Completed);
                if(
                    ReceivingIdentity.has_value()
                    && !completed_cleaned)
                {
                    completed_cleaned =
                        remove_exact_private_file(
                            Completed,
                            *ReceivingIdentity);
                }
                CompletedOwned = !completed_cleaned;
                cleaned = cleaned && completed_cleaned;
            }
            static_cast<void>(
                synchronize_directory(
                    Receiving.parent_path()));
            if(cleaned && Registered)
            {
                State->forget_operation(Operation);
            }
        }
        catch(...)
        {
        }
    }

private:
    std::shared_ptr<cache_state> State;
    core::epoch_operation_id Operation;
    std::filesystem::path Receiving;
    std::filesystem::path Completed;
    std::uint64_t MaximumBytes = 0U;
    core::git_hash_algorithm Algorithm =
        core::git_hash_algorithm::invalid;
    std::size_t DigestBytes = 0U;
    QCryptographicHash Hash;
    private_pack_file File;
    std::vector<std::uint8_t> Header;
    std::vector<std::byte> Tail;
    std::optional<exact_artifact_identity> ReceivingIdentity;
    std::uint64_t Bytes = 0U;
    bool Finished = false;
    bool Aborted = false;
    bool CompletedOwned = false;
    bool Registered = false;
};

[[nodiscard]] auto synchronize_directory(
    const std::filesystem::path& path) -> bool
{
#if defined(Q_OS_UNIX)
    const auto descriptor = ::open(
        path.c_str(),
        O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if(descriptor < 0)
    {
        return false;
    }
    const auto synchronized = ::fsync(descriptor) == 0;
    static_cast<void>(::close(descriptor));
    return synchronized;
#elif defined(Q_OS_WIN)
    static_cast<void>(path);
    return true;
#else
    static_cast<void>(path);
    return false;
#endif
}

[[nodiscard]] auto rename_no_replace(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::error_code& error) noexcept -> bool
{
    error.clear();
#if defined(Q_OS_MACOS)
    if(
        ::renamex_np(
            source.c_str(),
            destination.c_str(),
            RENAME_EXCL)
        == 0)
    {
        return true;
    }
    error = std::error_code{errno, std::generic_category()};
    return false;
#elif defined(Q_OS_LINUX)
    if(
        ::syscall(
            SYS_renameat2,
            AT_FDCWD,
            source.c_str(),
            AT_FDCWD,
            destination.c_str(),
            RENAME_NOREPLACE)
        == 0)
    {
        return true;
    }
    error = std::error_code{errno, std::generic_category()};
    return false;
#elif defined(Q_OS_WIN)
    if(
        MoveFileExW(
            source.c_str(),
            destination.c_str(),
            MOVEFILE_WRITE_THROUGH)
        != 0)
    {
        return true;
    }
    error = std::error_code{
        static_cast<int>(GetLastError()),
        std::system_category()};
    return false;
#else
    static_cast<void>(source);
    static_cast<void>(destination);
    error = std::make_error_code(
        std::errc::operation_not_supported);
    return false;
#endif
}

[[nodiscard]] auto random_quarantine_path(
    const std::filesystem::path& source,
    const std::string_view label)
    -> std::optional<std::filesystem::path>
{
    auto generator = QRandomGenerator::securelySeeded();
    for(std::size_t attempt = 0U; attempt < 8U; ++attempt)
    {
        auto nonce = std::array<std::uint8_t, 16U>{};
        for(std::size_t index = 0U; index < nonce.size(); index += 4U)
        {
            const auto value = generator.generate();
            nonce[index] = static_cast<std::uint8_t>(value >> 24U);
            nonce[index + 1U] =
                static_cast<std::uint8_t>(value >> 16U);
            nonce[index + 2U] =
                static_cast<std::uint8_t>(value >> 8U);
            nonce[index + 3U] = static_cast<std::uint8_t>(value);
        }
        const auto candidate =
            source.parent_path()
            / (std::string{label} + lower_hex(nonce));
        if(missing(candidate))
        {
            return candidate;
        }
    }
    return std::nullopt;
}

[[nodiscard]] auto rename_exact_to_new_path(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const exact_artifact_identity& expected) -> bool
{
    const auto private_source =
        expected.Kind == exact_artifact_kind::regular_file
        ? private_regular_file(source)
        : private_directory(source);
    if(
        !private_source
        || exact_identity(source, expected.Kind) != expected
        || !private_directory(source.parent_path())
        || !private_directory(destination.parent_path()))
    {
        return false;
    }
    std::error_code error;
    if(!rename_no_replace(source, destination, error))
    {
        return false;
    }
    return missing(source)
        && exact_identity(destination, expected.Kind) == expected
        && synchronize_directory(source.parent_path())
        && (source.parent_path() == destination.parent_path()
            || synchronize_directory(destination.parent_path()));
}

[[nodiscard]] auto remove_exact_private_file(
    const std::filesystem::path& path,
    const exact_artifact_identity& expected) -> bool
{
    if(missing(path))
    {
        return true;
    }
    if(
        expected.Kind != exact_artifact_kind::regular_file
        || exact_identity(path, expected.Kind) != expected
        || !private_regular_file(path))
    {
        return false;
    }
    const auto quarantine =
        random_quarantine_path(path, ".bpr-file-cleanup-");
    if(!quarantine.has_value())
    {
        return false;
    }
    const auto renamed =
        rename_exact_to_new_path(path, *quarantine, expected);
    if(
        !renamed
        && !(
            missing(path)
            && exact_identity(*quarantine, expected.Kind)
                == expected
            && synchronize_directory(path.parent_path())))
    {
        return false;
    }
    if(exact_identity(*quarantine, expected.Kind) != expected)
    {
        return false;
    }
    std::error_code error;
    return std::filesystem::remove(*quarantine, error) && !error
        && missing(*quarantine)
        && synchronize_directory(path.parent_path());
}

struct installed_pack_artifacts
{
    exact_artifact_identity Pack;
    exact_artifact_identity Index;

    auto operator<=>(const installed_pack_artifacts&) const = default;
};

[[nodiscard]] auto inspect_installed_pack_artifacts(
    const std::filesystem::path& repository,
    const application::git_pack_oid& pack)
    -> std::optional<installed_pack_artifacts>
{
    if(!application::valid(pack))
    {
        return std::nullopt;
    }
    const auto digest_bytes =
        pack.Algorithm == core::git_hash_algorithm::sha1 ? 20U : 32U;
    const auto pack_directory = repository / "objects" / "pack";
    const auto stem =
        "pack-"
        + lower_hex(
            std::span{pack.Digest}.first(digest_bytes));
    const auto pack_path = pack_directory / (stem + ".pack");
    const auto index_path = pack_directory / (stem + ".idx");
    const auto reverse_path = pack_directory / (stem + ".rev");
    const auto pack_identity = exact_identity(
        pack_path,
        exact_artifact_kind::regular_file);
    const auto index_identity = exact_identity(
        index_path,
        exact_artifact_kind::regular_file);
    return private_directory(pack_directory)
            && private_regular_file(pack_path)
            && private_regular_file(index_path)
            && missing(reverse_path) && pack_identity.has_value()
            && index_identity.has_value()
        ? std::optional<installed_pack_artifacts>{
              installed_pack_artifacts{
                  .Pack = *pack_identity,
                  .Index = *index_identity,
              }}
        : std::nullopt;
}

[[nodiscard]] auto rollback_new_pack(
    const std::filesystem::path& repository,
    const application::git_pack_oid& pack,
    const std::span<const application::git_pack_inventory_entry>
        preimport_packs,
    const installed_pack_artifacts& expected_artifacts) -> bool
{
    const auto digest_bytes =
        pack.Algorithm == core::git_hash_algorithm::sha1 ? 20U : 32U;
    if(
        !application::valid(pack)
        || std::ranges::any_of(
            preimport_packs,
            [&](const auto& entry)
            {
                return entry.Pack == pack;
            }))
    {
        return false;
    }
    const auto pack_directory = repository / "objects" / "pack";
    if(!private_directory(pack_directory))
    {
        return false;
    }
    const auto stem =
        "pack-"
        + lower_hex(
            std::span{pack.Digest}.first(digest_bytes));
    const auto pack_path = pack_directory / (stem + ".pack");
    const auto index_path = pack_directory / (stem + ".idx");
    const auto reverse_path = pack_directory / (stem + ".rev");
    if(
        exact_identity(
            pack_path,
            exact_artifact_kind::regular_file)
                != expected_artifacts.Pack
        || exact_identity(
               index_path,
               exact_artifact_kind::regular_file)
            != expected_artifacts.Index
        || !private_regular_file(pack_path)
        || !private_regular_file(index_path) || !missing(reverse_path))
    {
        return false;
    }
    const auto index_quarantine =
        random_quarantine_path(index_path, ".bpr-index-cleanup-");
    const auto pack_quarantine =
        random_quarantine_path(pack_path, ".bpr-pack-cleanup-");
    const auto move_exact =
        [](const std::filesystem::path& source,
           const std::filesystem::path& destination,
           const exact_artifact_identity& identity)
    {
        return rename_exact_to_new_path(
                   source,
                   destination,
                   identity)
            || (
                missing(source)
                && exact_identity(destination, identity.Kind)
                    == identity
                && synchronize_directory(
                    destination.parent_path()));
    };
    if(
        !index_quarantine.has_value()
        || !pack_quarantine.has_value()
        || !move_exact(
            index_path,
            *index_quarantine,
            expected_artifacts.Index))
    {
        return false;
    }
    if(
        !move_exact(
            pack_path,
            *pack_quarantine,
            expected_artifacts.Pack))
    {
        static_cast<void>(move_exact(
            *index_quarantine,
            index_path,
            expected_artifacts.Index));
        return false;
    }
    return remove_exact_private_file(
               *index_quarantine,
               expected_artifacts.Index)
        && remove_exact_private_file(
            *pack_quarantine,
            expected_artifacts.Pack)
        && synchronize_directory(pack_directory);
}

[[nodiscard]] auto deletion_marker_present(
    const std::filesystem::path& storage,
    const std::string_view cache_text)
    -> application::git_cache_result<bool>
{
    const auto quarantine = storage / "quarantine";
    const auto quarantine_v1 = quarantine / "v1";
    if(missing(quarantine))
    {
        return false;
    }
    if(!private_directory(quarantine))
    {
        return std::unexpected{
            application::git_cache_failure::invalid_path};
    }
    if(missing(quarantine_v1))
    {
        return false;
    }
    if(!private_directory(quarantine_v1))
    {
        return std::unexpected{
            application::git_cache_failure::invalid_path};
    }
    const auto marker =
        quarantine_v1 / (std::string{cache_text} + ".deleted");
    if(missing(marker))
    {
        return false;
    }
    if(!private_regular_file(marker))
    {
        return std::unexpected{
            application::git_cache_failure::repair_required};
    }
    return true;
}

[[nodiscard]] auto remove_tree_no_follow(
    const std::filesystem::path& root,
    const exact_artifact_identity& expected,
    const std::filesystem::path& fault_storage = {}) -> bool
{
    if(
        expected.Kind != exact_artifact_kind::directory
        || !private_directory(root)
        || exact_identity(root, expected.Kind) != expected)
    {
        return false;
    }
    struct tree_entry
    {
        std::filesystem::path Path;
        exact_artifact_identity Identity;
    };
    auto entries = std::vector<tree_entry>{};
    std::error_code error;
    auto entry_count = std::size_t{0U};
    auto iterator = std::filesystem::recursive_directory_iterator{
        root,
        std::filesystem::directory_options::none,
        error};
    const auto end = std::filesystem::recursive_directory_iterator{};
    while(!error && iterator != end)
    {
        const auto path = iterator->path();
        const auto status =
            std::filesystem::symlink_status(path, error);
        if(error)
        {
            break;
        }
        if(++entry_count > maximum_repository_entries)
        {
            return false;
        }
#if defined(Q_OS_WIN)
        const auto attributes = GetFileAttributesW(path.c_str());
        if(attributes == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        const auto link_like =
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
        const auto link_like = std::filesystem::is_symlink(status);
#endif
        if(link_like)
        {
            return false;
        }
        const auto kind =
            status.type()
                == std::filesystem::file_type::directory
            ? exact_artifact_kind::directory
            : exact_artifact_kind::regular_file;
        const auto identity = exact_identity(path, kind);
        if(
            !identity.has_value()
            || identity->Volume != expected.Volume)
        {
            return false;
        }
        entries.push_back({
            .Path = path,
            .Identity = *identity,
        });
        iterator.increment(error);
    }
    if(
        error
        || exact_identity(root, expected.Kind) != expected)
    {
        return false;
    }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
    std::error_code substitution_injection_error;
    if(
        !fault_storage.empty()
        && std::filesystem::exists(
            fault_storage
                / "inject-cache-delete-child-substitution",
            substitution_injection_error)
        && !substitution_injection_error)
    {
        const auto target = std::ranges::find_if(
            entries,
            [&](const auto& entry)
            {
                return entry.Path == root / manifest_name;
            });
        if(target == entries.end())
        {
            return false;
        }
        std::iter_swap(target, entries.end() - 1);
        const auto displaced =
            root / ".bpr-test-original-manifest";
        static constexpr std::array<std::uint8_t, 24U> canary{
            'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
            'e', 'n', 't', '-', 'c', 'h', 'i', 'l', 'd',
            '-', 'c', 'a', 'n', 'a', 'r', 'y',
        };
        if(
            !rename_exact_to_new_path(
                (entries.end() - 1)->Path,
                displaced,
                (entries.end() - 1)->Identity)
            || !write_new_private_file(
                (entries.end() - 1)->Path,
                canary))
        {
            return false;
        }
    }
    else if(substitution_injection_error)
    {
        return false;
    }
#else
    static_cast<void>(fault_storage);
#endif
    std::ranges::reverse(entries);
    for(const auto& entry : entries)
    {
        if(
            exact_identity(
                entry.Path,
                entry.Identity.Kind)
            != entry.Identity)
        {
            return false;
        }
        std::error_code remove_error;
        if(
            !std::filesystem::remove(
                entry.Path,
                remove_error)
            || remove_error)
        {
            return false;
        }
    }
    std::error_code root_error;
    return exact_identity(root, expected.Kind) == expected
        && std::filesystem::remove(root, root_error)
        && !root_error && missing(root);
}

[[nodiscard]] auto quarantine_and_remove_exact_tree(
    const std::filesystem::path& path,
    const exact_artifact_identity& expected) -> bool
{
    const auto quarantine =
        random_quarantine_path(path, ".bpr-tree-cleanup-");
    if(!quarantine.has_value())
    {
        return false;
    }
    const auto renamed =
        rename_exact_to_new_path(path, *quarantine, expected);
    if(
        !renamed
        && !(
            missing(path)
            && exact_identity(*quarantine, expected.Kind)
                == expected
            && synchronize_directory(path.parent_path())))
    {
        return false;
    }
    return remove_tree_no_follow(*quarantine, expected)
        && synchronize_directory(path.parent_path());
}

class installed_git_cache_session final
    : public application::git_cache_session
{
public:
    explicit installed_git_cache_session(
        std::shared_ptr<cache_state> state)
        : State{std::move(state)}
    {
    }

    [[nodiscard]] auto request() const noexcept
        -> const application::git_cache_open_request& override
    {
        return State->Request;
    }

    [[nodiscard]] auto create_pack_sink(
        const core::epoch_operation_id operation,
        const std::uint64_t maximum_bytes,
        const std::stop_token stop)
        -> application::git_cache_result<
            std::unique_ptr<application::git_pack_stream_sink>> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(
            stop.stop_requested() || !operation.valid()
            || maximum_bytes == 0U
            || maximum_bytes > State->Request.Limits.OperationPackBytes)
        {
            return std::unexpected{
                stop.stop_requested()
                    ? application::git_cache_failure::cancelled
                    : application::git_cache_failure::
                          invalid_request};
        }
        const auto inventory = State->validated_inventory(stop);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        const auto completed =
            State->Process->pack_spool_path(
                process_operation(operation));
        if(!completed.has_value())
        {
            return std::unexpected{
                State->process_failure(completed.error())};
        }
        const auto receiving =
            completed->parent_path()
            / ("." + lower_hex(operation.bytes())
                + ".pack.receiving");
        if(!missing(*completed) || !missing(receiving))
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        auto sink = std::make_unique<pack_sink>(
            State,
            operation,
            receiving,
            *completed,
            maximum_bytes,
            State->Request.ObjectFormat);
        if(!sink->initialize())
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        return std::unique_ptr<application::git_pack_stream_sink>{
            std::move(sink)};
    }

    [[nodiscard]] auto observe_preimport(
        const application::completed_git_pack_receipt& receipt,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_preimport_observation> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(
            !application::agrees(receipt)
            || State->operation_aborted(receipt.Operation)
            || receipt.ExpectedPack.Algorithm
                != State->Request.ObjectFormat
            || receipt.Spool.Bytes
                > State->Request.Limits.OperationPackBytes)
        {
            return std::unexpected{
                application::git_cache_failure::
                    inventory_mismatch};
        }
        const auto recorded = State->find_receipt(receipt);
        if(
            !recorded.has_value()
            || !private_regular_file(recorded->Spool)
            || exact_identity(
                   recorded->Spool,
                   exact_artifact_kind::regular_file)
                != recorded->SpoolIdentity)
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        const auto inventory = State->validated_inventory(stop);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        const auto preexisting = std::ranges::any_of(
            inventory->Packs,
            [&](const auto& entry)
            {
                return entry.Pack == receipt.ExpectedPack;
            });
        const auto ref_bytes =
            ref_pair_bytes(State->Request.ObjectFormat);
        auto admission_bytes = ref_bytes;
        if(!preexisting)
        {
            if(
                receipt.Spool.Bytes
                    > std::numeric_limits<std::uint64_t>::max()
                        - recorded->MaximumIndexBytes
                || receipt.Spool.Bytes
                        + recorded->MaximumIndexBytes
                    > std::numeric_limits<std::uint64_t>::max()
                        - admission_bytes)
            {
                return std::unexpected{
                    application::git_cache_failure::quota_exceeded};
            }
            admission_bytes +=
                receipt.Spool.Bytes
                + recorded->MaximumIndexBytes;
        }
        if(
            !fits_within(
                inventory->RepositoryBytes,
                admission_bytes,
                State->Request.Limits.RepositoryBytes)
            || (inventory->Packs.size()
                    >= State->Request.Limits.PackCount
                && !preexisting))
        {
            return std::unexpected{
                application::git_cache_failure::quota_exceeded};
        }
        auto observation =
            application::git_preimport_observation{
                .Operation = receipt.Operation,
                .ExpectedPack = receipt.ExpectedPack,
                .Packs = inventory->Packs,
                .RepositoryBytes = inventory->RepositoryBytes,
                .ExpectedPackPreexisting = preexisting,
                .OpaqueToken = State->new_token(),
            };
        if(
            !State->register_observation({
                .Observation = observation,
                .Receipt = receipt,
                .MaximumIndexBytes = recorded->MaximumIndexBytes,
            }))
        {
            return std::unexpected{
                application::git_cache_failure::
                    inventory_mismatch};
        }
        return observation;
    }

    [[nodiscard]] auto prepare_revision(
        const application::git_prepare_revision_request& request,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_prepared_revision> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(!application::valid(request))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_request};
        }
        const auto recorded_observation = State->find_observation(
            request.Inventory,
            request.Receipt);
        if(
            request.Base.algorithm() != State->Request.ObjectFormat
            || State->operation_aborted(
                request.Receipt.Operation)
            || request.Authorization.ExpectedPack.Algorithm
                != State->Request.ObjectFormat
            || !recorded_observation.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::
                    authorization_mismatch};
        }
        const auto current = State->validated_inventory(stop);
        if(
            !current.has_value()
            || current->Packs != request.Inventory.Packs
            || current->RepositoryBytes
                != request.Inventory.RepositoryBytes)
        {
            return std::unexpected{
                current.has_value()
                    ? application::git_cache_failure::
                          inventory_mismatch
                    : current.error()};
        }
        const auto ref_bytes =
            ref_pair_bytes(State->Request.ObjectFormat);
        auto admission_bytes = ref_bytes;
        if(!request.Inventory.ExpectedPackPreexisting)
        {
            if(
                request.Receipt.Spool.Bytes
                    > std::numeric_limits<std::uint64_t>::max()
                        - recorded_observation->MaximumIndexBytes
                || request.Receipt.Spool.Bytes
                        + recorded_observation->MaximumIndexBytes
                    > std::numeric_limits<std::uint64_t>::max()
                        - admission_bytes)
            {
                return std::unexpected{
                    application::git_cache_failure::quota_exceeded};
            }
            admission_bytes +=
                request.Receipt.Spool.Bytes
                + recorded_observation->MaximumIndexBytes;
        }
        if(
            current->PrivateRefs
                > State->Request.Limits.PrivateRefCount - 2U
            || !fits_within(
                current->RepositoryBytes,
                admission_bytes,
                State->Request.Limits.RepositoryBytes)
            || (current->Packs.size()
                    >= State->Request.Limits.PackCount
                && !request.Inventory.ExpectedPackPreexisting))
        {
            return std::unexpected{
                application::git_cache_failure::quota_exceeded};
        }
        const auto process_expected =
            process_pack(request.Receipt.ExpectedPack);
        const auto base = process_commit(request.Base);
        const auto head = process_commit(request.Head);
        if(
            !process_expected.has_value() || !base.has_value()
            || !head.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::
                    object_mismatch};
        }
        const auto recorded_receipt =
            State->find_receipt(request.Receipt);
        if(
            !recorded_receipt.has_value()
            || !private_regular_file(recorded_receipt->Spool)
            || exact_identity(
                   recorded_receipt->Spool,
                   exact_artifact_kind::regular_file)
                != recorded_receipt->SpoolIdentity)
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        auto indexed = State->Process->index_pack(
            application::git_index_pack_request{
                .Repository = State->Repository,
                .Operation =
                    process_operation(request.Receipt.Operation),
                .SpoolBytes = request.Receipt.Spool.Bytes,
                .MaximumInputBytes =
                    State->Request.Limits.OperationPackBytes,
                .ExpectedPack = *process_expected,
            },
            stop);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
        std::error_code lock_substitution_injection_error;
        if(
            std::filesystem::exists(
                State->StorageRoot
                    / "inject-cache-lock-substitution-during-index",
                lock_substitution_injection_error)
            && !lock_substitution_injection_error)
        {
            const auto displaced_lock =
                State->LockPath.parent_path()
                / (".bpr-test-original-"
                    + State->LockPath.filename().string());
            static constexpr std::array<std::uint8_t, 23U>
                canary{
                    'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                    'e', 'n', 't', '-', 'l', 'o', 'c', 'k',
                    '-', 'c', 'a', 'n', 'a', 'r', 'y',
                };
            if(
                !rename_exact_to_new_path(
                    State->LockPath,
                    displaced_lock,
                    State->LockIdentity)
                || !write_new_private_file(
                    State->LockPath,
                    canary))
            {
                if(
                    !indexed.has_value()
                    && indexed.error()
                        == application::git_process_failure::
                            reap_failure)
                {
                    static_cast<void>(
                        State->process_failure(indexed.error()));
                }
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
        }
        else if(lock_substitution_injection_error)
        {
            if(
                !indexed.has_value()
                && indexed.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(indexed.error()));
            }
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
#endif
        if(!State->active_boundary_still_owned())
        {
            if(
                !indexed.has_value()
                && indexed.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(indexed.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto imported_artifacts =
            request.Inventory.ExpectedPackPreexisting
            ? std::optional<installed_pack_artifacts>{}
            : inspect_installed_pack_artifacts(
                  State->Repository,
                  request.Receipt.ExpectedPack);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
        std::error_code injection_error;
        if(
            indexed.has_value()
            && std::filesystem::exists(
                State->StorageRoot
                    / "inject-cache-index-terminal-failure",
                injection_error)
            && !injection_error)
        {
            indexed = std::unexpected{
                application::git_process_failure::malformed_output};
        }
#endif
        if(!indexed.has_value())
        {
            const auto failure =
                State->process_failure(indexed.error());
            if(
                indexed.error()
                == application::git_process_failure::reap_failure)
            {
                return std::unexpected{failure};
            }
            const auto after_failure =
                State->validated_inventory({}, false);
            if(!after_failure.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            if(same_inventory(*current, *after_failure))
            {
                return std::unexpected{failure};
            }
            const auto prior_packs_unchanged =
                std::ranges::all_of(
                    current->Packs,
                    [&](const auto& prior)
                    {
                        return std::ranges::find(
                                   after_failure->Packs,
                                   prior)
                            != after_failure->Packs.end();
                    });
            const auto expected_present =
                std::ranges::any_of(
                    after_failure->Packs,
                    [&](const auto& entry)
                    {
                        return entry.Pack
                            == request.Receipt.ExpectedPack;
                    });
            const auto only_expected_pack_added =
                !request.Inventory.ExpectedPackPreexisting
                && imported_artifacts.has_value()
                && expected_present && prior_packs_unchanged
                && after_failure->Packs.size()
                    == current->Packs.size() + 1U
                && after_failure->PrivateRefs
                    == current->PrivateRefs;
            if(
                !only_expected_pack_added
                || !rollback_new_pack(
                    State->Repository,
                    request.Receipt.ExpectedPack,
                    current->Packs,
                    *imported_artifacts))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            const auto reconciled =
                State->validated_inventory({}, false);
            return reconciled.has_value()
                    && same_inventory(*current, *reconciled)
                ? application::git_cache_result<
                      application::git_prepared_revision>{
                      std::unexpected{failure}}
                : application::git_cache_result<
                      application::git_prepared_revision>{
                      std::unexpected{
                          application::git_cache_failure::
                              repair_required}};
        }
        if(
            !request.Inventory.ExpectedPackPreexisting
            && !imported_artifacts.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto rollback_failure =
            [&](const application::git_cache_failure failure)
                -> application::git_cache_result<
                    application::git_prepared_revision>
        {
            if(request.Inventory.ExpectedPackPreexisting)
            {
                return std::unexpected{failure};
            }
            const auto rolled_back = rollback_new_pack(
                State->Repository,
                request.Receipt.ExpectedPack,
                current->Packs,
                *imported_artifacts);
            const auto reconciled = rolled_back
                ? State->validated_inventory({}, false)
                : application::git_cache_result<
                      inventory_snapshot>{
                      std::unexpected{
                          application::git_cache_failure::
                              repair_required}};
            return reconciled.has_value()
                    && same_inventory(*current, *reconciled)
                ? application::git_cache_result<
                      application::git_prepared_revision>{
                      std::unexpected{failure}}
                : application::git_cache_result<
                      application::git_prepared_revision>{
                      std::unexpected{
                          application::git_cache_failure::
                              repair_required}};
        };
        if(indexed->Pack != *process_expected)
        {
            return rollback_failure(
                application::git_cache_failure::object_mismatch);
        }

        const auto after_import =
            State->validated_inventory({}, false);
        if(!after_import.has_value())
        {
            return rollback_failure(after_import.error());
        }
        const auto expected_pack = std::ranges::find_if(
            after_import->Packs,
            [&](const auto& entry)
            {
                return entry.Pack == request.Receipt.ExpectedPack;
            });
        const auto prior_packs_unchanged = std::ranges::all_of(
            current->Packs,
            [&](const auto& prior)
            {
                return std::ranges::find(
                           after_import->Packs,
                           prior)
                    != after_import->Packs.end();
            });
        const auto expected_pack_count =
            current->Packs.size()
            + (request.Inventory.ExpectedPackPreexisting ? 0U : 1U);
        if(
            expected_pack == after_import->Packs.end()
            || !prior_packs_unchanged
            || after_import->Packs.size() != expected_pack_count
            || after_import->Packs.size()
                > State->Request.Limits.PackCount
            || !fits_within(
                after_import->RepositoryBytes,
                ref_bytes,
                State->Request.Limits.RepositoryBytes)
            || after_import->PrivateRefs
                > State->Request.Limits.PrivateRefCount - 2U)
        {
            return rollback_failure(
                application::git_cache_failure::quota_exceeded);
        }
        const auto prepared_artifacts =
            inspect_installed_pack_artifacts(
                State->Repository,
                request.Receipt.ExpectedPack);
        if(
            !prepared_artifacts.has_value()
            || (
                imported_artifacts.has_value()
                && *prepared_artifacts
                    != *imported_artifacts))
        {
            return rollback_failure(
                application::git_cache_failure::
                    repair_required);
        }
        const auto durable_pack_identity =
            artifact_identity(prepared_artifacts->Pack);
        const auto durable_index_identity =
            artifact_identity(prepared_artifacts->Index);
        if(
            !durable_pack_identity.has_value()
            || !durable_index_identity.has_value())
        {
            return rollback_failure(
                application::git_cache_failure::
                    repair_required);
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto base_inspection =
            State->Process->inspect_commit(
                application::git_commit_request{
                    .Repository = State->Repository,
                    .Commit = *base,
                },
                stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !base_inspection.has_value()
                && base_inspection.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(
                        base_inspection.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!base_inspection.has_value())
        {
            if(
                base_inspection.error()
                != application::git_process_failure::reap_failure)
            {
                return rollback_failure(
                    State->process_failure(
                        base_inspection.error()));
            }
            return std::unexpected{
                State->process_failure(
                    base_inspection.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto head_inspection =
            State->Process->inspect_commit(
                application::git_commit_request{
                    .Repository = State->Repository,
                    .Commit = *head,
                },
                stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !head_inspection.has_value()
                && head_inspection.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(
                        head_inspection.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!head_inspection.has_value())
        {
            if(
                head_inspection.error()
                != application::git_process_failure::reap_failure)
            {
                return rollback_failure(
                    State->process_failure(
                        head_inspection.error()));
            }
            return std::unexpected{
                State->process_failure(
                    head_inspection.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto connected = State->Process->verify_connectivity(
            application::git_connectivity_request{
                .Repository = State->Repository,
                .Base = *base,
                .Head = *head,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !connected.has_value()
                && connected.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(connected.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!connected.has_value())
        {
            if(
                connected.error()
                != application::git_process_failure::reap_failure)
            {
                return rollback_failure(
                    State->process_failure(connected.error()));
            }
            return std::unexpected{
                State->process_failure(connected.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const std::array<application::git_ref_mutation, 2U>
            staging{
                application::git_ref_write{
                    .Ref = staging_ref(
                        request.Receipt.Operation,
                        application::git_private_ref_endpoint::base),
                    .DesiredObject = *base,
                    .ExpectedOld =
                        application::git_ref_expected_absent{},
                },
                application::git_ref_write{
                    .Ref = staging_ref(
                        request.Receipt.Operation,
                        application::git_private_ref_endpoint::head),
                    .DesiredObject = *head,
                    .ExpectedOld =
                        application::git_ref_expected_absent{},
                },
            };
        const auto staged = State->Process->update_refs(
            application::git_update_refs_request{
                .Repository = State->Repository,
                .Mutations = staging,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !staged.has_value()
                && staged.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(staged.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!staged.has_value())
        {
            // A cancelled or failed update-ref child may have crossed its
            // commit point before its terminal output was observed. Retain
            // the admitted pack and completed spool for exact recovery;
            // deleting either here could strand staging refs.
            return std::unexpected{
                State->process_failure(staged.error())};
        }
        if(
            !remove_exact_private_file(
                recorded_receipt->Spool,
                recorded_receipt->SpoolIdentity))
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        if(
            !State->active_boundary_still_owned()
            || inspect_installed_pack_artifacts(
                   State->Repository,
                   request.Receipt.ExpectedPack)
                != prepared_artifacts)
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        return application::git_prepared_revision{
            .Operation = request.Receipt.Operation,
            .Pack = request.Receipt.ExpectedPack,
            .InstalledPack =
                {
                    .Role =
                        application::git_artifact_role::
                            installed_pack,
                    .Identity = *durable_pack_identity,
                    .Bytes = expected_pack->PackBytes,
                },
            .InstalledIndex =
                {
                    .Role =
                        application::git_artifact_role::
                            installed_index,
                    .Identity = *durable_index_identity,
                    .Bytes = expected_pack->IndexBytes,
                },
            .Base = request.Base,
            .BaseTree = core_tree(base_inspection->Tree),
            .Head = request.Head,
            .HeadTree = core_tree(head_inspection->Tree),
            .ImportedBytes = request.Receipt.Spool.Bytes,
        };
    }

    [[nodiscard]] auto promote_retained(
        const application::git_prepared_revision& prepared,
        const application::git_ref_promotion_authorization&
            authorization,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_retained_ref_pair> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(
            !application::valid(prepared)
            || !application::valid(authorization)
            || prepared.Operation != authorization.Operation
            || State->operation_aborted(prepared.Operation)
            || prepared.Base != authorization.Base
            || prepared.Head != authorization.Head
            || authorization.Scope
                != State->Request.Authority.Scope
            || authorization.Base.algorithm()
                != State->Request.ObjectFormat)
        {
            return std::unexpected{
                application::git_cache_failure::
                    authorization_mismatch};
        }
        const auto inventory = State->validated_inventory(stop);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        const auto installed = inspect_installed_pack_artifacts(
            State->Repository,
            prepared.Pack);
        const auto installed_inventory =
            std::ranges::find_if(
                inventory->Packs,
                [&](const auto& entry)
                {
                    return entry.Pack == prepared.Pack;
                });
        if(
            !installed.has_value()
            || installed_inventory
                == inventory->Packs.end()
            || exact_identity(
                   prepared.InstalledPack.Identity)
                != installed->Pack
            || exact_identity(
                   prepared.InstalledIndex.Identity)
                != installed->Index
            || prepared.InstalledPack.Bytes
                != installed_inventory->PackBytes
            || prepared.InstalledIndex.Bytes
                != installed_inventory->IndexBytes)
        {
            return std::unexpected{
                application::git_cache_failure::
                    authorization_mismatch};
        }
        const auto base = process_commit(prepared.Base);
        const auto head = process_commit(prepared.Head);
        if(!base.has_value() || !head.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::
                    object_mismatch};
        }
        const auto inspected = inspect_pair(
            staging_ref(
                prepared.Operation,
                application::git_private_ref_endpoint::base),
            staging_ref(
                prepared.Operation,
                application::git_private_ref_endpoint::head),
            stop);
        if(
            !inspected.has_value() || inspected->first != *base
            || inspected->second != *head)
        {
            return std::unexpected{
                inspected.has_value()
                    ? application::git_cache_failure::ref_conflict
                    : inspected.error()};
        }
        const auto final_base = retained_ref(
            authorization.Scope,
            authorization.Ordinal,
            application::git_private_ref_endpoint::base);
        const auto final_head = retained_ref(
            authorization.Scope,
            authorization.Ordinal,
            application::git_private_ref_endpoint::head);
        const auto absent = inspect_pair_optional(
            final_base,
            final_head,
            stop);
        if(
            !absent.has_value() || absent->first.has_value()
            || absent->second.has_value())
        {
            return std::unexpected{
                absent.has_value()
                    ? application::git_cache_failure::ref_conflict
                    : absent.error()};
        }
        const std::array<application::git_ref_mutation, 4U>
            mutations{
                application::git_ref_write{
                    .Ref = final_base,
                    .DesiredObject = *base,
                    .ExpectedOld =
                        application::git_ref_expected_absent{},
                },
                application::git_ref_write{
                    .Ref = final_head,
                    .DesiredObject = *head,
                    .ExpectedOld =
                        application::git_ref_expected_absent{},
                },
                application::git_ref_remove{
                    .Ref = staging_ref(
                        prepared.Operation,
                        application::git_private_ref_endpoint::base),
                    .ExpectedOld = *base,
                },
                application::git_ref_remove{
                    .Ref = staging_ref(
                        prepared.Operation,
                        application::git_private_ref_endpoint::head),
                    .ExpectedOld = *head,
                },
            };
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto promoted = State->Process->update_refs(
            application::git_update_refs_request{
                .Repository = State->Repository,
                .Mutations = mutations,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !promoted.has_value()
                && promoted.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(promoted.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!promoted.has_value())
        {
            return std::unexpected{
                State->process_failure(promoted.error())};
        }
        State->forget_operation(prepared.Operation);
        return application::git_retained_ref_pair{
            .Scope = authorization.Scope,
            .Ordinal = authorization.Ordinal,
            .Base = prepared.Base,
            .Head = prepared.Head,
        };
    }

    [[nodiscard]] auto inspect_retained(
        const core::epoch_ordinal ordinal,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_retained_ref_pair> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(!core::valid(ordinal))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_request};
        }
        const auto inventory = State->validated_inventory(stop, false);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        const auto pair = inspect_pair(
            retained_ref(
                State->Request.Authority.Scope,
                ordinal,
                application::git_private_ref_endpoint::base),
            retained_ref(
                State->Request.Authority.Scope,
                ordinal,
                application::git_private_ref_endpoint::head),
            stop);
        if(!pair.has_value())
        {
            return std::unexpected{pair.error()};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto base_inspection =
            State->Process->inspect_commit(
                application::git_commit_request{
                    .Repository = State->Repository,
                    .Commit = pair->first,
                },
                stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !base_inspection.has_value()
                && base_inspection.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(
                        base_inspection.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!base_inspection.has_value())
        {
            return std::unexpected{
                State->process_failure(
                    base_inspection.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto head_inspection =
            State->Process->inspect_commit(
                application::git_commit_request{
                    .Repository = State->Repository,
                    .Commit = pair->second,
                },
                stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !head_inspection.has_value()
                && head_inspection.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(
                        head_inspection.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!head_inspection.has_value())
        {
            return std::unexpected{
                State->process_failure(
                    head_inspection.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto connected = State->Process->verify_connectivity(
            application::git_connectivity_request{
                .Repository = State->Repository,
                .Base = pair->first,
                .Head = pair->second,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !connected.has_value()
                && connected.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(connected.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!connected.has_value())
        {
            return std::unexpected{
                State->process_failure(connected.error())};
        }
        return application::git_retained_ref_pair{
            .Scope = State->Request.Authority.Scope,
            .Ordinal = ordinal,
            .Base = core_commit(pair->first),
            .Head = core_commit(pair->second),
        };
    }

    [[nodiscard]] auto prune_retained(
        const application::git_ref_prune_authorization&
            authorization,
        const std::stop_token stop)
        -> application::git_cache_result<void> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(
            !application::valid(authorization)
            || authorization.Expected.Scope
                != State->Request.Authority.Scope
            || authorization.Expected.Base.algorithm()
                != State->Request.ObjectFormat)
        {
            return std::unexpected{
                application::git_cache_failure::
                    authorization_mismatch};
        }
        const auto inventory = State->validated_inventory(stop, false);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        const auto base =
            process_commit(authorization.Expected.Base);
        const auto head =
            process_commit(authorization.Expected.Head);
        if(!base.has_value() || !head.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::
                    object_mismatch};
        }
        const auto pair = inspect_pair(
            retained_ref(
                authorization.Expected.Scope,
                authorization.Expected.Ordinal,
                application::git_private_ref_endpoint::base),
            retained_ref(
                authorization.Expected.Scope,
                authorization.Expected.Ordinal,
                application::git_private_ref_endpoint::head),
            stop);
        if(
            !pair.has_value() || pair->first != *base
            || pair->second != *head)
        {
            return std::unexpected{
                pair.has_value()
                    ? application::git_cache_failure::ref_conflict
                    : pair.error()};
        }
        const std::array<application::git_ref_mutation, 2U>
            mutations{
                application::git_ref_remove{
                    .Ref = retained_ref(
                        authorization.Expected.Scope,
                        authorization.Expected.Ordinal,
                        application::git_private_ref_endpoint::base),
                    .ExpectedOld = *base,
                },
                application::git_ref_remove{
                    .Ref = retained_ref(
                        authorization.Expected.Scope,
                        authorization.Expected.Ordinal,
                        application::git_private_ref_endpoint::head),
                    .ExpectedOld = *head,
                },
            };
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto pruned = State->Process->update_refs(
            application::git_update_refs_request{
                .Repository = State->Repository,
                .Mutations = mutations,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !pruned.has_value()
                && pruned.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(pruned.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        return pruned.has_value()
            ? application::git_cache_result<void>{}
            : std::unexpected{
                  State->process_failure(pruned.error())};
    }

    [[nodiscard]] auto abort_operation(
        const core::epoch_operation_id operation,
        const std::stop_token stop)
        -> application::git_cache_result<void> override
    {
        std::lock_guard operation_guard{State->OperationMutex};
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(!operation.valid())
        {
            return std::unexpected{
                application::git_cache_failure::invalid_request};
        }
        const auto inventory = State->validated_inventory(stop, false);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        if(!State->mark_operation_aborted(operation))
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        const auto pair = inspect_pair_optional(
            staging_ref(
                operation,
                application::git_private_ref_endpoint::base),
            staging_ref(
                operation,
                application::git_private_ref_endpoint::head),
            stop);
        if(!pair.has_value())
        {
            return std::unexpected{pair.error()};
        }
        if(
            pair->first.has_value() != pair->second.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        if(pair->first.has_value())
        {
            const std::array<application::git_ref_mutation, 2U>
                mutations{
                    application::git_ref_remove{
                        .Ref = staging_ref(
                            operation,
                            application::git_private_ref_endpoint::base),
                        .ExpectedOld = *pair->first,
                    },
                    application::git_ref_remove{
                        .Ref = staging_ref(
                            operation,
                            application::git_private_ref_endpoint::head),
                        .ExpectedOld = *pair->second,
                    },
                };
            if(!State->active_boundary_still_owned())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            const auto removed = State->Process->update_refs(
                application::git_update_refs_request{
                    .Repository = State->Repository,
                    .Mutations = mutations,
                },
                stop);
            if(!State->active_boundary_still_owned())
            {
                if(
                    !removed.has_value()
                    && removed.error()
                        == application::git_process_failure::
                            reap_failure)
                {
                    static_cast<void>(
                        State->process_failure(removed.error()));
                }
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            if(!removed.has_value())
            {
                return std::unexpected{
                State->process_failure(removed.error())};
            }
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        const auto spool = State->Process->pack_spool_path(
            process_operation(operation));
        if(!spool.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        const auto receiving =
            spool->parent_path()
            / ("." + lower_hex(operation.bytes())
                + ".pack.receiving");
        const auto record = State->find_spool(operation);
        if(!record.has_value())
        {
            if(!missing(*spool) || !missing(receiving))
            {
                return std::unexpected{
                    application::git_cache_failure::spool_failure};
            }
            State->forget_operation(operation);
            return {};
        }
        if(
            record->Completed != *spool
            || record->Receiving != receiving
            || (!missing(*spool)
                && !remove_exact_private_file(
                    *spool,
                    record->Identity))
            || (!missing(receiving)
                && !remove_exact_private_file(
                    receiving,
                    record->Identity)))
        {
            return std::unexpected{
                application::git_cache_failure::spool_failure};
        }
        State->forget_operation(operation);
        return {};
    }

private:
    [[nodiscard]] auto inspect_pair_optional(
        const application::git_private_ref& base_ref,
        const application::git_private_ref& head_ref,
        const std::stop_token stop)
        -> application::git_cache_result<
            std::pair<
                std::optional<application::git_commit_id>,
                std::optional<application::git_commit_id>>>
    {
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto base = State->Process->inspect_ref(
            application::git_inspect_ref_request{
                .Repository = State->Repository,
                .Ref = base_ref,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !base.has_value()
                && base.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(base.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!base.has_value())
        {
            return std::unexpected{
                State->process_failure(base.error())};
        }
        if(!State->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }
        const auto head = State->Process->inspect_ref(
            application::git_inspect_ref_request{
                .Repository = State->Repository,
                .Ref = head_ref,
            },
            stop);
        if(!State->active_boundary_still_owned())
        {
            if(
                !head.has_value()
                && head.error()
                    == application::git_process_failure::
                        reap_failure)
            {
                static_cast<void>(
                    State->process_failure(head.error()));
            }
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!head.has_value())
        {
            return std::unexpected{
                State->process_failure(head.error())};
        }
        return std::pair{*base, *head};
    }

    [[nodiscard]] auto inspect_pair(
        const application::git_private_ref& base_ref,
        const application::git_private_ref& head_ref,
        const std::stop_token stop)
        -> application::git_cache_result<
            std::pair<
                application::git_commit_id,
                application::git_commit_id>>
    {
        const auto pair =
            inspect_pair_optional(base_ref, head_ref, stop);
        if(
            !pair.has_value() || !pair->first.has_value()
            || !pair->second.has_value())
        {
            return std::unexpected{
                pair.has_value()
                    ? application::git_cache_failure::ref_conflict
                    : pair.error()};
        }
        return std::pair{*pair->first, *pair->second};
    }

    std::shared_ptr<cache_state> State;
};

class installed_git_cache final : public application::git_cache_port
{
public:
    explicit installed_git_cache(
        installed_git_cache_configuration configuration)
        : Configuration{std::move(configuration)}
    {
    }

    [[nodiscard]] auto open(
        const application::git_cache_open_request& request,
        const std::stop_token stop)
        -> application::git_cache_result<
            std::unique_ptr<application::git_cache_session>> override
    {
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(!application::valid(request))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_request};
        }
        const auto storage =
            canonical_existing(Configuration.StorageRoot);
        if(!storage.has_value() || !private_directory(*storage))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_path};
        }
        if(
            !make_private_directory(*storage / "locks")
            || !make_private_directory(*storage / "locks" / "v1"))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_path};
        }
        const auto cache_text =
            lower_hex(request.Authority.Cache.bytes());
        const auto repository =
            *storage / "repositories" / "v1"
            / (cache_text + ".git");
        const auto lock_path =
            *storage / "locks" / "v1" / (cache_text + ".lock");
        auto lock = std::make_unique<cache_lock>();
        if(!lock->acquire(lock_path))
        {
            return std::unexpected{
                application::git_cache_failure::lock_unavailable};
        }
        const auto lock_identity = lock->identity();
        if(
            !lock_identity.has_value()
            || !lock->still_names(lock_path))
        {
            return std::unexpected{
                application::git_cache_failure::lock_unavailable};
        }
        const auto deleted =
            deletion_marker_present(*storage, cache_text);
        if(!deleted.has_value())
        {
            return std::unexpected{deleted.error()};
        }
        if(*deleted)
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        const auto repository_absent = missing(repository);
        auto repository_identity =
            std::optional<exact_artifact_identity>{};
        if(!repository_absent)
        {
            repository_identity = exact_identity(
                repository,
                exact_artifact_kind::directory);
            if(
                !repository_identity.has_value()
                || !private_directory(repository))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
        }
        auto process = make_installed_git_process(
            {
                .GitExecutable = Configuration.GitExecutable,
                .IsolationRoot = *storage,
                .WindowsSystemRoot = Configuration.WindowsSystemRoot,
            },
            stop);
        if(!process.has_value())
        {
            return std::unexpected{
                process.error()
                        == application::git_process_failure::cancelled
                    ? application::git_cache_failure::cancelled
                    : application::git_cache_failure::
                          process_failure};
        }

        auto state = std::make_shared<cache_state>(
            request,
            *storage,
            repository,
            repository_identity,
            lock_path,
            *lock_identity,
            std::move(lock),
            std::move(*process));
        if(repository_absent)
        {
            const auto algorithm =
                process_algorithm(request.ObjectFormat);
            if(!algorithm.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_request};
            }
            auto staging = std::filesystem::path{};
            for(std::size_t attempt = 0U; attempt < 8U; ++attempt)
            {
                const auto candidate =
                    repository.parent_path()
                    / (lower_hex(state->new_token()) + ".git");
                if(candidate != repository && missing(candidate))
                {
                    staging = candidate;
                    break;
                }
            }
            if(staging.empty())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            const auto initialized =
                state->Process->initialize_bare_repository(
                    application::git_initialize_repository_request{
                        .Repository = staging,
                        .Algorithm = *algorithm,
                    },
                    stop);
            if(!initialized.has_value())
            {
                return std::unexpected{
                    state->process_failure(initialized.error())};
            }
            const auto staging_identity = exact_identity(
                staging,
                exact_artifact_kind::directory);
            if(!staging_identity.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            if(!state->bind_repository(staging, *staging_identity))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            const auto manifest =
                manifest_bytes(request, *lock_identity);
            auto publish_manifest = true;
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
            std::error_code manifest_injection_error;
            publish_manifest = !std::filesystem::exists(
                *storage / "inject-cache-manifest-failure",
                manifest_injection_error);
            if(manifest_injection_error)
            {
                publish_manifest = false;
            }
#endif
            if(
                !publish_manifest
                || !write_new_private_file(
                    staging / manifest_name,
                    manifest))
            {
                return quarantine_and_remove_exact_tree(
                           staging,
                           *staging_identity)
                    ? application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                              application::git_cache_failure::
                                  invalid_manifest}}
                    : application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                                  application::git_cache_failure::
                                      repair_required}};
            }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
            std::error_code validation_injection_error;
            if(
                std::filesystem::exists(
                    *storage
                        / "inject-cache-post-manifest-substitution",
                    validation_injection_error)
                && !validation_injection_error)
            {
                const auto displaced =
                    staging.parent_path()
                    / ".bpr-test-displaced-cache-staging.git";
                static constexpr std::array<std::uint8_t, 29U>
                    canary{
                        'p', 'o', 's', 't', '-', 'm', 'a', 'n',
                        'i', 'f', 'e', 's', 't', '-', 'r', 'o',
                        'o', 't', '-', 'c', 'a', 'n', 'a', 'r',
                        'y', '-', 'd', 'a', 't',
                    };
                if(
                    !rename_exact_to_new_path(
                        staging,
                        displaced,
                        *staging_identity)
                    || !make_private_directory(staging)
                    || !write_new_private_file(
                        staging / "replacement-canary",
                        canary))
                {
                    return std::unexpected{
                        application::git_cache_failure::
                            repair_required};
                }
            }
            else if(validation_injection_error)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
#endif
            if(!state->validate_topology())
            {
                return quarantine_and_remove_exact_tree(
                           staging,
                           *staging_identity)
                    ? application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                              application::git_cache_failure::
                                  invalid_manifest}}
                    : application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                              application::git_cache_failure::
                                  repair_required}};
            }
            if(stop.stop_requested())
            {
                return quarantine_and_remove_exact_tree(
                           staging,
                           *staging_identity)
                    ? application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                              application::git_cache_failure::
                                  cancelled}}
                    : application::git_cache_result<
                          std::unique_ptr<
                              application::git_cache_session>>{
                          std::unexpected{
                              application::git_cache_failure::
                                  repair_required}};
            }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
            std::error_code destination_injection_error;
            if(
                std::filesystem::exists(
                    *storage
                        / "inject-cache-final-destination",
                    destination_injection_error)
                && !destination_injection_error)
            {
                static_cast<void>(
                    make_private_directory(repository));
                static constexpr std::array<std::uint8_t, 6U>
                    canary{0x63U, 0x61U, 0x6eU, 0x61U, 0x72U, 0x79U};
                static_cast<void>(write_new_private_file(
                    repository / "canary",
                    canary));
            }
#endif
            const auto renamed = rename_exact_to_new_path(
                staging,
                repository,
                *staging_identity);
            if(!renamed)
            {
                const auto published_identity = exact_identity(
                    repository,
                    exact_artifact_kind::directory);
                if(
                    published_identity != staging_identity
                    || !missing(staging)
                    || !synchronize_directory(
                        repository.parent_path()))
                {
                    if(
                        exact_identity(
                            staging,
                            exact_artifact_kind::directory)
                        == staging_identity)
                    {
                        static_cast<void>(
                            quarantine_and_remove_exact_tree(
                                staging,
                                *staging_identity));
                    }
                    return std::unexpected{
                        application::git_cache_failure::
                            repair_required};
                }
            }
            if(!state->bind_repository(
                repository,
                *staging_identity))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
        }
        else if(!state->active_boundary_still_owned())
        {
            return std::unexpected{
                application::git_cache_failure::
                    invalid_repository};
        }

        if(!state->validate_topology(stop))
        {
            return std::unexpected{
                stop.stop_requested()
                    ? application::git_cache_failure::cancelled
                    : application::git_cache_failure::
                          invalid_manifest};
        }
        const auto inventory = state->inventory(stop, false);
        if(!inventory.has_value())
        {
            return std::unexpected{inventory.error()};
        }
        return std::unique_ptr<application::git_cache_session>{
            std::make_unique<installed_git_cache_session>(
                std::move(state))};
    }

    [[nodiscard]] auto delete_cache(
        const application::git_cache_open_request& expected,
        const application::git_cache_delete_authorization&
            authorization,
        const std::stop_token stop)
        -> application::git_cache_result<void> override
    {
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_cache_failure::cancelled};
        }
        if(
            !application::valid(expected)
            || !application::valid(authorization)
            || authorization.ApplicationSession
                != expected.ApplicationSession
            || authorization.Cache != expected.Authority.Cache)
        {
            return std::unexpected{
                application::git_cache_failure::
                    authorization_mismatch};
        }
        const auto storage =
            canonical_existing(Configuration.StorageRoot);
        if(!storage.has_value() || !private_directory(*storage))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_path};
        }
        if(
            !make_private_directory(*storage / "locks")
            || !make_private_directory(*storage / "locks" / "v1")
            || !make_private_directory(*storage / "quarantine")
            || !make_private_directory(
                *storage / "quarantine" / "v1"))
        {
            return std::unexpected{
                application::git_cache_failure::invalid_path};
        }
        const auto cache_text =
            lower_hex(expected.Authority.Cache.bytes());
        const auto repository =
            *storage / "repositories" / "v1"
            / (cache_text + ".git");
        const auto lock_path =
            *storage / "locks" / "v1" / (cache_text + ".lock");
        const auto quarantine_root =
            *storage / "quarantine" / "v1";
        const auto quarantine =
            quarantine_root
            / (cache_text + "-"
                + lower_hex(authorization.DurableIntentToken)
                + ".git");
        const auto tombstone =
            quarantine_root / (cache_text + ".deleted");
        auto lock = std::make_unique<cache_lock>();
        if(!lock->acquire(lock_path))
        {
            return std::unexpected{
                application::git_cache_failure::lock_unavailable};
        }
        const auto lock_identity = lock->identity();
        if(
            !lock_identity.has_value()
            || !lock->still_names(lock_path))
        {
            return std::unexpected{
                application::git_cache_failure::lock_unavailable};
        }
        const auto held_lock_still_names = [&]
        {
            return lock->identity() == lock_identity
                && lock->still_names(lock_path);
        };
        const auto tombstone_missing = missing(tombstone);
        auto tombstone_record =
            std::optional<deletion_tombstone_record>{};
        if(!tombstone_missing)
        {
            const auto placeholder = deletion_tombstone_bytes(
                expected,
                authorization,
                deletion_tombstone_record{
                    .Repository =
                        {
                            .Kind =
                                exact_artifact_kind::directory,
                        },
                    .Lock =
                        {
                            .Kind =
                                exact_artifact_kind::regular_file,
                        },
                });
            const auto actual = read_private_file(
                tombstone,
                placeholder.size());
            if(!actual.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        authorization_mismatch};
            }
            tombstone_record = parse_deletion_tombstone(
                *actual,
                expected,
                authorization);
            if(!tombstone_record.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        authorization_mismatch};
            }
            if(tombstone_record->Lock != *lock_identity)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
        }
        const auto repository_present = private_directory(repository);
        const auto quarantine_present = private_directory(quarantine);
        if(
            (repository_present && quarantine_present)
            || (!repository_present && !missing(repository))
            || (!quarantine_present && !missing(quarantine))
            || (tombstone_missing && quarantine_present))
        {
            return std::unexpected{
                application::git_cache_failure::
                    repair_required};
        }
        if(!repository_present && !quarantine_present)
        {
            if(tombstone_missing)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        invalid_repository};
            }
            return held_lock_still_names()
                ? application::git_cache_result<void>{}
                : application::git_cache_result<void>{
                      std::unexpected{
                          application::git_cache_failure::
                              repair_required}};
        }

        if(tombstone_missing)
        {
            const auto validated_path =
                repository_present ? repository : quarantine;
            const auto repository_identity = exact_identity(
                validated_path,
                exact_artifact_kind::directory);
            if(!repository_identity.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            auto state = std::make_shared<cache_state>(
                expected,
                *storage,
                validated_path,
                *repository_identity,
                lock_path,
                *lock_identity,
                nullptr,
                nullptr);
            const auto inventory =
                state->validated_inventory(stop, false);
            if(!inventory.has_value())
            {
                return std::unexpected{inventory.error()};
            }
            if(
                exact_identity(
                    validated_path,
                    exact_artifact_kind::directory)
                != repository_identity)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
            tombstone_record = deletion_tombstone_record{
                .Repository = *repository_identity,
                .Lock = *lock_identity,
            };
            const auto expected_tombstone =
                deletion_tombstone_bytes(
                    expected,
                    authorization,
                    *tombstone_record);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
            std::error_code lock_substitution_injection_error;
            if(
                std::filesystem::exists(
                    *storage
                        / "inject-cache-lock-substitution-during-delete",
                    lock_substitution_injection_error)
                && !lock_substitution_injection_error)
            {
                const auto displaced_lock =
                    lock_path.parent_path()
                    / (".bpr-test-original-delete-"
                        + lock_path.filename().string());
                static constexpr std::array<std::uint8_t, 30U>
                    canary{
                        'r', 'e', 'p', 'l', 'a', 'c', 'e', 'm',
                        'e', 'n', 't', '-', 'd', 'e', 'l', 'e',
                        't', 'e', '-', 'l', 'o', 'c', 'k', '-',
                        'c', 'a', 'n', 'a', 'r', 'y',
                    };
                if(
                    !rename_exact_to_new_path(
                        lock_path,
                        displaced_lock,
                        *lock_identity)
                    || !write_new_private_file(
                        lock_path,
                        canary))
                {
                    return std::unexpected{
                        application::git_cache_failure::
                            repair_required};
                }
            }
            else if(lock_substitution_injection_error)
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
#endif
            if(
                !held_lock_still_names()
                || !write_new_private_file(
                    tombstone,
                    expected_tombstone)
                || !synchronize_directory(quarantine_root))
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
        }
        if(!tombstone_record.has_value())
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        const auto active_path =
            repository_present ? repository : quarantine;
        if(
            exact_identity(
                active_path,
                exact_artifact_kind::directory)
            != tombstone_record->Repository)
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        if(repository_present && !tombstone_missing)
        {
            auto state = std::make_shared<cache_state>(
                expected,
                *storage,
                repository,
                tombstone_record->Repository,
                lock_path,
                *lock_identity,
                nullptr,
                nullptr);
            const auto inventory =
                state->validated_inventory({}, false);
            if(!inventory.has_value())
            {
                return std::unexpected{
                    application::git_cache_failure::
                        repair_required};
            }
        }
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
        std::error_code delete_destination_injection_error;
        if(
            repository_present
            && std::filesystem::exists(
                *storage
                    / "inject-cache-delete-destination-race",
                delete_destination_injection_error)
            && !delete_destination_injection_error)
        {
            static_cast<void>(make_private_directory(quarantine));
            static constexpr std::array<std::uint8_t, 6U>
                canary{0x63U, 0x61U, 0x6eU, 0x61U, 0x72U, 0x79U};
            static_cast<void>(write_new_private_file(
                quarantine / "canary",
                canary));
        }
#endif
        if(
            !held_lock_still_names()
            || (repository_present
            && !rename_exact_to_new_path(
                repository,
                quarantine,
                tombstone_record->Repository)
            && !(
                missing(repository)
                && exact_identity(
                    quarantine,
                    exact_artifact_kind::directory)
                    == tombstone_record->Repository
                && synchronize_directory(quarantine_root))))
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        if(
            !held_lock_still_names()
            || !remove_tree_no_follow(
                quarantine,
                tombstone_record->Repository,
                *storage)
            || !synchronize_directory(quarantine.parent_path()))
        {
            return std::unexpected{
                application::git_cache_failure::repair_required};
        }
        return held_lock_still_names()
            ? application::git_cache_result<void>{}
            : application::git_cache_result<void>{
                  std::unexpected{
                      application::git_cache_failure::
                          repair_required}};
    }

private:
    installed_git_cache_configuration Configuration;
};
}

auto make_installed_git_cache(
    const installed_git_cache_configuration& configuration)
    -> std::expected<
        std::unique_ptr<application::git_cache_port>,
        application::git_cache_failure>
{
    const auto root = canonical_existing(configuration.StorageRoot);
    const auto executable =
        canonical_existing(configuration.GitExecutable);
    if(
        !root.has_value() || !executable.has_value()
        || !private_directory(*root))
    {
        return std::unexpected{
            application::git_cache_failure::invalid_path};
    }
    return std::unique_ptr<application::git_cache_port>{
        std::make_unique<installed_git_cache>(configuration)};
}
}
