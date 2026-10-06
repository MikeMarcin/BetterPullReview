#include "bpr/adapters/git/installed_git_cache.hpp"

#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/platform/windows_sharing_retry.hpp"

// The cache manifest discriminates the access mode, and `epoch_model.hpp`
// declares that enumeration without its enumerators. This is the header that
// defines them.
#include "bpr/core/anonymous_read_policy.hpp"

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
	#include <sys/attr.h>
#endif

#if defined(Q_OS_LINUX)
	#include <linux/fs.h>
	#include <sys/syscall.h>
#endif

#if defined(Q_OS_WIN)
	#include <qt_windows.h>
#endif

namespace bpr::adapters::git {
	namespace {
		namespace application = bpr::application;
		namespace core        = bpr::core;

		inline constexpr std::string_view              manifest_name = "betterpullreview-cache-manifest-v1";
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
		inline constexpr std::size_t   maximum_repository_entries = 1'000'000U;
		inline constexpr std::uint64_t maximum_pack_index_bytes   = 4ULL * 1024ULL * 1024ULL * 1024ULL;
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
			const std::uint64_t aCurrent,
			const std::uint64_t aAddition,
			const std::uint64_t aLimit) noexcept -> bool
		{
			return aCurrent <= aLimit && aAddition <= aLimit - aCurrent;
		}

		[[nodiscard]] constexpr auto ref_pair_bytes(
			const core::git_hash_algorithm aAlgorithm) noexcept -> std::uint64_t
		{
			return aAlgorithm == core::git_hash_algorithm::sha1 ? 82U : 130U;
		}

		[[nodiscard]] auto maximum_index_bytes(
			const core::git_hash_algorithm      aAlgorithm,
			const std::span<const std::uint8_t> aHeader)
			-> std::optional<std::uint64_t>
		{
			if (aHeader.size() != 12U) {
				return std::nullopt;
			}
			const auto objectCount = (static_cast<std::uint64_t>(aHeader[8U]) << 24U)
				| (static_cast<std::uint64_t>(aHeader[9U]) << 16U)
				| (static_cast<std::uint64_t>(aHeader[10U]) << 8U)
				| static_cast<std::uint64_t>(aHeader[11U]);
			const auto digestBytes = aAlgorithm == core::git_hash_algorithm::sha1
				? 20U
				: aAlgorithm == core::git_hash_algorithm::sha256 ? 32U
																 : 0U;
			if (digestBytes == 0U) {
				return std::nullopt;
			}

			// Git index v2: header, 256-entry fanout, object IDs, CRCs, 32-bit
			// offsets, a worst-case 64-bit offset for every object, and two hashes.
			const auto fixedBytes     = 8ULL + 256ULL * 4ULL + 2ULL * digestBytes;
			const auto perObjectBytes = digestBytes + 4ULL + 4ULL + 8ULL;
			if (
				objectCount
				> (std::numeric_limits<std::uint64_t>::max() - fixedBytes)
					/ perObjectBytes) {
				return std::nullopt;
			}
			const auto result = fixedBytes + objectCount * perObjectBytes;
			return result <= maximum_pack_index_bytes
				? std::optional<std::uint64_t>{ result }
				: std::nullopt;
		}

		[[nodiscard]] auto lower_hex(
			const std::span<const std::uint8_t> aBytes) -> std::string
		{
			constexpr std::string_view digits = "0123456789abcdef";
			auto                       result = std::string{ };
			result.reserve(aBytes.size() * 2U);
			for (const auto byte : aBytes) {
				result.push_back(digits[byte >> 4U]);
				result.push_back(digits[byte & 0x0fU]);
			}
			return result;
		}

		[[nodiscard]] auto hex_value(const char aValue) noexcept
			-> std::optional<std::uint8_t>
		{
			if (aValue >= '0' && aValue <= '9') {
				return static_cast<std::uint8_t>(aValue - '0');
			}
			if (aValue >= 'a' && aValue <= 'f') {
				return static_cast<std::uint8_t>(aValue - 'a' + 10);
			}
			return std::nullopt;
		}

		[[nodiscard]] auto parse_pack_oid(
			const std::string_view         aText,
			const core::git_hash_algorithm aAlgorithm)
			-> std::optional<application::git_pack_oid>
		{
			const auto byteCount = aAlgorithm == core::git_hash_algorithm::sha1
				? 20U
				: aAlgorithm == core::git_hash_algorithm::sha256 ? 32U
																 : 0U;
			if (aText.size() != byteCount * 2U) {
				return std::nullopt;
			}
			auto result = application::git_pack_oid{ .Algorithm = aAlgorithm };
			for (std::size_t index = 0U; index < byteCount; ++index) {
				const auto high = hex_value(aText[index * 2U]);
				const auto low  = hex_value(aText[index * 2U + 1U]);
				if (!high.has_value() || !low.has_value()) {
					return std::nullopt;
				}
				result.Digest[index] = static_cast<std::uint8_t>(
					static_cast<std::uint8_t>(*high << 4U) | *low);
			}
			return application::valid(result)
				? std::optional<application::git_pack_oid>{ result }
				: std::nullopt;
		}

		[[nodiscard]] auto canonical_existing(
			const std::filesystem::path& aPath)
			-> std::optional<std::filesystem::path>
		{
			std::error_code error;
			if (aPath.empty() || !aPath.is_absolute()) {
				return std::nullopt;
			}
			auto canonical = std::filesystem::canonical(aPath, error);
			if (error || canonical != aPath.lexically_normal()) {
				return std::nullopt;
			}
			return canonical;
		}

		[[nodiscard]] auto private_directory(
			const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (
				error || status.type() != std::filesystem::file_type::directory
				|| std::filesystem::is_symlink(status)
				|| canonical_existing(aPath) != aPath) {
				return false;
			}
#if defined(Q_OS_UNIX)
			struct stat information{ };
			if (::lstat(aPath.c_str(), &information) != 0) {
				return false;
			}
			return S_ISDIR(information.st_mode)
				&& information.st_uid == ::geteuid()
				&& (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0;
#elif defined(Q_OS_WIN)
			const auto attributes = GetFileAttributesW(aPath.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES
				&& (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U
				&& (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
#else
			return false;
#endif
		}

		[[nodiscard]] auto private_regular_file(
			const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (
				error
				|| status.type() != std::filesystem::file_type::regular
				|| std::filesystem::is_symlink(status)
				|| canonical_existing(aPath) != aPath) {
				return false;
			}
#if defined(Q_OS_UNIX)
			struct stat information{ };
			if (::lstat(aPath.c_str(), &information) != 0) {
				return false;
			}
			return S_ISREG(information.st_mode)
				&& information.st_uid == ::geteuid()
				&& information.st_nlink == 1
				&& (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0;
#elif defined(Q_OS_WIN)
			// Attributes alone cannot see a hard link: the POSIX rule above
			// requires exactly one name (st_nlink == 1), and NTFS answers the
			// same question through the open handle. Attribute-only access is
			// exempt from sharing checks, so a spool the sink still holds is
			// still inspectable (BPR-0437).
			const auto handle = CreateFileW(
				aPath.c_str(),
				FILE_READ_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return false;
			}
			BY_HANDLE_FILE_INFORMATION information{ };
			const auto                 valid
				= GetFileInformationByHandle(handle, &information) != 0;
			static_cast<void>(CloseHandle(handle));
			return valid
				&& (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				== 0U
				&& (information.dwFileAttributes
					& FILE_ATTRIBUTE_REPARSE_POINT)
				== 0U
				&& information.nNumberOfLinks == 1U;
#else
			return false;
#endif
		}

		enum class exact_artifact_kind : std::uint8_t {
			regular_file,
			directory,
		};

		struct exact_artifact_identity
		{
			exact_artifact_kind Kind       = exact_artifact_kind::regular_file;
			std::uint64_t       Volume     = 0U;
			std::uint64_t       ObjectHigh = 0U;
			std::uint64_t       ObjectLow  = 0U;

			auto operator<=>(const exact_artifact_identity&) const = default;
		};

#if defined(Q_OS_UNIX)
		[[nodiscard]] auto exact_identity(
			const struct stat&        aInformation,
			const exact_artifact_kind aKind)
			-> std::optional<exact_artifact_identity>
		{
			const auto expectedType = aKind == exact_artifact_kind::regular_file
				? S_ISREG(aInformation.st_mode)
				: S_ISDIR(aInformation.st_mode);
			if (
				!expectedType || aInformation.st_uid != ::geteuid()
				|| aInformation.st_nlink == 0
				|| (aInformation.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) != 0
				|| (aKind == exact_artifact_kind::regular_file
					&& aInformation.st_nlink != 1)) {
				return std::nullopt;
			}
			return exact_artifact_identity{
				.Kind       = aKind,
				.Volume     = static_cast<std::uint64_t>(aInformation.st_dev),
				.ObjectHigh = 0U,
				.ObjectLow  = static_cast<std::uint64_t>(aInformation.st_ino),
			};
		}

		[[nodiscard]] auto exact_identity(
			const int                 aDescriptor,
			const exact_artifact_kind aKind)
			-> std::optional<exact_artifact_identity>
		{
			struct stat information{ };
			return ::fstat(aDescriptor, &information) == 0
				? exact_identity(information, aKind)
				: std::nullopt;
		}
#elif defined(Q_OS_WIN)
		[[nodiscard]] auto exact_identity(
			const HANDLE              handle,
			const exact_artifact_kind kind)
			-> std::optional<exact_artifact_identity>
		{
			FILE_ATTRIBUTE_TAG_INFO    tag{ };
			BY_HANDLE_FILE_INFORMATION information{ };
			if (
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
					&& information.nNumberOfLinks != 1U)) {
				return std::nullopt;
			}
			const auto directory = (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
			if (
				directory
				!= (kind == exact_artifact_kind::directory)) {
				return std::nullopt;
			}
			return exact_artifact_identity{
				.Kind   = kind,
				.Volume = static_cast<std::uint64_t>(
					information.dwVolumeSerialNumber),
				.ObjectHigh = static_cast<std::uint64_t>(
					information.nFileIndexHigh),
				.ObjectLow = static_cast<std::uint64_t>(
					information.nFileIndexLow),
			};
		}
#endif

		[[nodiscard]] auto exact_identity(
			const std::filesystem::path& aPath,
			const exact_artifact_kind    aKind)
			-> std::optional<exact_artifact_identity>
		{
#if defined(Q_OS_UNIX)
			struct stat information{ };
			return ::lstat(aPath.c_str(), &information) == 0
				? exact_identity(information, aKind)
				: std::nullopt;
#elif defined(Q_OS_WIN)
			const auto handle = CreateFileW(
				aPath.c_str(),
				FILE_READ_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT
					| FILE_FLAG_BACKUP_SEMANTICS,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return std::nullopt;
			}
			const auto result = exact_identity(handle, aKind);
			static_cast<void>(CloseHandle(handle));
			return result;
#else
			static_cast<void>(aPath);
			static_cast<void>(aKind);
			return std::nullopt;
#endif
		}

		[[nodiscard]] auto missing(const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			return (
					   !error
					   && status.type()
						   == std::filesystem::file_type::not_found)
				|| error
				== std::errc::no_such_file_or_directory;
		}

		[[nodiscard]] auto make_private_directory(
			const std::filesystem::path& aPath) -> bool
		{
			if (private_directory(aPath)) {
				return true;
			}
			if (!missing(aPath) || !private_directory(aPath.parent_path())) {
				return false;
			}
			std::error_code error;
			if (!std::filesystem::create_directory(aPath, error) || error) {
				return false;
			}
			std::filesystem::permissions(
				aPath,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace,
				error);
			return !error && private_directory(aPath);
		}

		[[nodiscard]] auto read_private_file(
			const std::filesystem::path& aPath,
			const std::size_t            aMaximumBytes)
			-> std::optional<std::vector<std::uint8_t>>
		{
			if (!private_regular_file(aPath)) {
				return std::nullopt;
			}
			std::error_code error;
			const auto      size = std::filesystem::file_size(aPath, error);
			if (error || size > aMaximumBytes) {
				return std::nullopt;
			}
			auto result = std::vector<std::uint8_t>(
				static_cast<std::size_t>(size));
#if defined(Q_OS_UNIX)
			const auto descriptor = ::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			if (descriptor < 0) {
				return std::nullopt;
			}
			std::size_t offset = 0U;
			while (offset < result.size()) {
				const auto received = ::read(
					descriptor,
					result.data() + offset,
					result.size() - offset);
				if (received < 0 && errno == EINTR) {
					continue;
				}
				if (received <= 0) {
					static_cast<void>(::close(descriptor));
					return std::nullopt;
				}
				offset += static_cast<std::size_t>(received);
			}
			struct stat information{ };
			const auto  valid = ::fstat(descriptor, &information) == 0
				&& S_ISREG(information.st_mode)
				&& information.st_uid == ::geteuid()
				&& information.st_nlink == 1
				&& information.st_size >= 0
				&& static_cast<std::uint64_t>(information.st_size) == size
				&& (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0;
			static_cast<void>(::close(descriptor));
			return valid
				? std::optional<std::vector<std::uint8_t>>{ std::move(result) }
				: std::nullopt;
#elif defined(Q_OS_WIN)
			const auto handle = CreateFileW(
				aPath.c_str(),
				GENERIC_READ,
				0U,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return std::nullopt;
			}
			std::size_t offset = 0U;
			while (offset < result.size()) {
				const auto requested = static_cast<DWORD>(
					std::min<std::size_t>(
						result.size() - offset,
						std::numeric_limits<DWORD>::max()));
				DWORD received = 0U;
				if (
					ReadFile(
						handle,
						result.data() + offset,
						requested,
						&received,
						nullptr)
						== 0
					|| received == 0U) {
					static_cast<void>(CloseHandle(handle));
					return std::nullopt;
				}
				offset += received;
			}
			static_cast<void>(CloseHandle(handle));
			return result;
#else
			static_cast<void>(aMaximumBytes);
			return std::nullopt;
#endif
		}

		[[nodiscard]] auto write_new_private_file(
			const std::filesystem::path&        aPath,
			const std::span<const std::uint8_t> aBytes) -> bool
		{
			if (
				aPath.empty() || !aPath.is_absolute()
				|| !private_directory(aPath.parent_path())) {
				return false;
			}
#if defined(Q_OS_UNIX)
			const auto descriptor = ::open(
				aPath.c_str(),
				O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				S_IRUSR | S_IWUSR);
			if (descriptor < 0) {
				return false;
			}
			std::size_t offset = 0U;
			while (offset < aBytes.size()) {
				const auto written = ::write(
					descriptor,
					aBytes.data() + offset,
					aBytes.size() - offset);
				if (written < 0 && errno == EINTR) {
					continue;
				}
				if (written <= 0) {
					static_cast<void>(::close(descriptor));
					return false;
				}
				offset += static_cast<std::size_t>(written);
			}
			const auto synchronized = ::fsync(descriptor) == 0;
			static_cast<void>(::close(descriptor));
			return synchronized && private_regular_file(aPath);
#elif defined(Q_OS_WIN)
			const auto handle = CreateFileW(
				aPath.c_str(),
				GENERIC_WRITE,
				0U,
				nullptr,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
					| FILE_FLAG_WRITE_THROUGH,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return false;
			}
			std::size_t offset = 0U;
			while (offset < aBytes.size()) {
				const auto requested = static_cast<DWORD>(
					std::min<std::size_t>(
						aBytes.size() - offset,
						std::numeric_limits<DWORD>::max()));
				DWORD written = 0U;
				if (
					WriteFile(
						handle,
						aBytes.data() + offset,
						requested,
						&written,
						nullptr)
						== 0
					|| written == 0U) {
					static_cast<void>(CloseHandle(handle));
					return false;
				}
				offset += written;
			}
			const auto synchronized = FlushFileBuffers(handle) != 0;
			static_cast<void>(CloseHandle(handle));
			return synchronized && private_regular_file(aPath);
#else
			static_cast<void>(aBytes);
			return false;
#endif
		}

		auto append_u64(
			std::vector<std::uint8_t>& aOutput,
			const std::uint64_t        aValue) -> void
		{
			for (unsigned int shift = 56U;; shift -= 8U) {
				aOutput.push_back(
					static_cast<std::uint8_t>(aValue >> shift));
				if (shift == 0U) {
					break;
				}
			}
		}

		[[nodiscard]] auto read_u64(
			const std::span<const std::uint8_t> aInput,
			const std::size_t                   aOffset) -> std::optional<std::uint64_t>
		{
			if (aOffset > aInput.size() || aInput.size() - aOffset < 8U) {
				return std::nullopt;
			}
			auto result = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < 8U; ++index) {
				result = static_cast<std::uint64_t>(
					(result << 8U) | aInput[aOffset + index]);
			}
			return result;
		}

		auto append_identity(
			std::vector<std::uint8_t>&     aOutput,
			const exact_artifact_identity& aIdentity) -> void
		{
			append_u64(aOutput, aIdentity.Volume);
			append_u64(aOutput, aIdentity.ObjectHigh);
			append_u64(aOutput, aIdentity.ObjectLow);
		}

		[[nodiscard]] auto read_identity(
			const std::span<const std::uint8_t> aInput,
			const std::size_t                   aOffset,
			const exact_artifact_kind           aKind)
			-> std::optional<exact_artifact_identity>
		{
			const auto volume = read_u64(aInput, aOffset);
			const auto high   = read_u64(aInput, aOffset + 8U);
			const auto low    = read_u64(aInput, aOffset + 16U);
			if (!volume.has_value() || !high.has_value() || !low.has_value()) {
				return std::nullopt;
			}
			return exact_artifact_identity{
				.Kind       = aKind,
				.Volume     = *volume,
				.ObjectHigh = *high,
				.ObjectLow  = *low,
			};
		}

		[[nodiscard]] auto artifact_identity(
			const exact_artifact_identity& aIdentity)
			-> std::optional<application::git_artifact_identity>
		{
			if (aIdentity.Kind != exact_artifact_kind::regular_file) {
				return std::nullopt;
			}
			auto result      = application::git_artifact_identity{ };
			result.Bytes[0U] = 1U;
			result.Bytes[1U] = 1U;
			result.Bytes[2U] = artifact_identity_scheme;
			const auto writeU64 =
				[&](const std::size_t aOffset, const std::uint64_t aValue) {
					for (std::size_t index = 0U; index < 8U; ++index) {
						result.Bytes[aOffset + index] = static_cast<std::uint8_t>(
							aValue >> (56U - index * 8U));
					}
				};
			writeU64(8U, aIdentity.Volume);
			writeU64(16U, aIdentity.ObjectHigh);
			writeU64(24U, aIdentity.ObjectLow);
			return application::valid(result)
				? std::optional<application::git_artifact_identity>{ result }
				: std::nullopt;
		}

		[[nodiscard]] auto exact_identity(
			const application::git_artifact_identity& aIdentity)
			-> std::optional<exact_artifact_identity>
		{
			if (
				!application::valid(aIdentity)
				|| aIdentity.Bytes[1U] != 1U
				|| aIdentity.Bytes[2U]
					!= artifact_identity_scheme
				|| std::ranges::any_of(
					std::span{ aIdentity.Bytes }.subspan(3U, 5U),
					[](const auto aByte) {
						return aByte != 0U;
					})) {
				return std::nullopt;
			}
			const auto bytes  = std::span{ aIdentity.Bytes };
			const auto volume = read_u64(bytes, 8U);
			const auto high   = read_u64(bytes, 16U);
			const auto low    = read_u64(bytes, 24U);
			return volume.has_value() && high.has_value()
					&& low.has_value()
				? std::optional<exact_artifact_identity>{
					  exact_artifact_identity{
						  .Kind       = exact_artifact_kind::regular_file,
						  .Volume     = *volume,
						  .ObjectHigh = *high,
						  .ObjectLow  = *low,
					  }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto manifest_authority_bytes(
			const application::git_cache_open_request& aRequest)
			-> std::vector<std::uint8_t>
		{
			auto result = std::vector<std::uint8_t>{ };
			result.reserve(12U + 1U + 6U * 8U + 3U * 32U);
			result.insert(
				result.end(),
				manifest_magic.begin(),
				manifest_magic.end());
			result.push_back(
				aRequest.ObjectFormat == core::git_hash_algorithm::sha1
					? 1U
					: 2U);
			append_u64(result, aRequest.Authority.Profile.value());
			append_u64(result, aRequest.Authority.ProfileRevision.value());
			append_u64(result, aRequest.Authority.Host.value());
			result.insert(
				result.end(),
				aRequest.Authority.HostAuthority.bytes().begin(),
				aRequest.Authority.HostAuthority.bytes().end());
			append_u64(result, aRequest.Authority.Account.value());
			append_u64(result, aRequest.Authority.Repository.value());
			result.insert(
				result.end(),
				aRequest.Authority.Cache.bytes().begin(),
				aRequest.Authority.Cache.bytes().end());
			// These bytes are the manifest that gates reuse of an existing
			// on-disk repository, so the access mode belongs in them: without
			// it a signed-out materialization of a repository and a signed-in
			// one carrying the same cache scope would present the same
			// authority, and the second would silently adopt the first's
			// objects and refs. With it, reopening either under the other's
			// mode is `invalid_manifest` — a typed refusal, not a reuse.
			//
			// The byte is appended only for a non-authenticated authority, the
			// same rule `core::append_authority` uses on the comparison
			// digest. A manifest already written by an authenticated session
			// is therefore byte-identical to what this produces, so an
			// existing cache keeps being reusable across the upgrade instead
			// of failing closed and forcing a full rematerialization.
			if (
				aRequest.Authority.Mode
				!= core::github_access_mode::authenticated) {
				result.push_back(
					static_cast<std::uint8_t>(aRequest.Authority.Mode));
			}
			return result;
		}

		[[nodiscard]] auto manifest_bytes(
			const application::git_cache_open_request& aRequest,
			const exact_artifact_identity&             aLockIdentity,
			const bool                                 aDurableVolume)
			-> std::vector<std::uint8_t>
		{
			auto result = manifest_authority_bytes(aRequest);
			if (aDurableVolume) {
				result[11U] = 2U;
			}
			result.reserve(result.size() + 24U);
			append_identity(result, aLockIdentity);
			return result;
		}

		struct deletion_tombstone_record
		{
			exact_artifact_identity Repository{
				.Kind = exact_artifact_kind::directory
			};
			exact_artifact_identity Lock{
				.Kind = exact_artifact_kind::regular_file
			};
		};

		[[nodiscard]] auto deletion_tombstone_bytes(
			const application::git_cache_open_request&         aRequest,
			const application::git_cache_delete_authorization& aAuthorization,
			const deletion_tombstone_record&                   aRecord)
			-> std::vector<std::uint8_t>
		{
			const auto authority = manifest_authority_bytes(aRequest);
			auto       result    = std::vector<std::uint8_t>{ };
			result.reserve(
				deletion_tombstone_magic.size() + authority.size()
				+ aAuthorization.DurableIntentToken.size() + 48U);
			result.insert(
				result.end(),
				deletion_tombstone_magic.begin(),
				deletion_tombstone_magic.end());
			result.insert(result.end(), authority.begin(), authority.end());
			result.insert(
				result.end(),
				aAuthorization.DurableIntentToken.begin(),
				aAuthorization.DurableIntentToken.end());
			append_identity(result, aRecord.Repository);
			append_identity(result, aRecord.Lock);
			return result;
		}

		[[nodiscard]] auto parse_deletion_tombstone(
			const std::span<const std::uint8_t>                aBytes,
			const application::git_cache_open_request&         aRequest,
			const application::git_cache_delete_authorization& aAuthorization)
			-> std::optional<deletion_tombstone_record>
		{
			const auto authority = manifest_authority_bytes(aRequest);
			auto       prefix    = std::vector<std::uint8_t>{ };
			prefix.reserve(
				deletion_tombstone_magic.size() + authority.size()
				+ aAuthorization.DurableIntentToken.size());
			prefix.insert(
				prefix.end(),
				deletion_tombstone_magic.begin(),
				deletion_tombstone_magic.end());
			prefix.insert(prefix.end(), authority.begin(), authority.end());
			prefix.insert(
				prefix.end(),
				aAuthorization.DurableIntentToken.begin(),
				aAuthorization.DurableIntentToken.end());
			if (
				aBytes.size() != prefix.size() + 48U
				|| !std::equal(prefix.begin(), prefix.end(), aBytes.begin())) {
				return std::nullopt;
			}
			const auto repository = read_identity(
				aBytes,
				prefix.size(),
				exact_artifact_kind::directory);
			const auto lock = read_identity(
				aBytes,
				prefix.size() + 24U,
				exact_artifact_kind::regular_file);
			return repository.has_value() && lock.has_value()
				? std::optional<deletion_tombstone_record>{
					  deletion_tombstone_record{
						  .Repository = *repository,
						  .Lock       = *lock,
					  }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto repository_config(
			const core::git_hash_algorithm aAlgorithm) -> std::string_view
		{
			return aAlgorithm == core::git_hash_algorithm::sha1
				? std::string_view{
					  "[core]\n"
					  "\trepositoryformatversion = 0\n"
					  "\tbare = true\n"
					  "\tlogallrefupdates = false\n"
				  }
				: std::string_view{ "[core]\n"
									"\trepositoryformatversion = 1\n"
									"\tbare = true\n"
									"\tlogallrefupdates = false\n"
									"[extensions]\n"
									"\tobjectformat = sha256\n" };
		}

		[[nodiscard]] auto read_exact_private_text(
			const std::filesystem::path& aPath,
			const std::string_view       aExpected) -> bool
		{
			const auto actual = read_private_file(aPath, aExpected.size());
			return actual.has_value() && actual->size() == aExpected.size()
				&& std::equal(
					   actual->begin(),
					   actual->end(),
					   reinterpret_cast<const std::uint8_t*>(
						   aExpected.data()));
		}

		class cache_lock final
		{
		public:
			cache_lock()                                     = default;
			cache_lock(const cache_lock&)                    = delete;
			auto operator=(const cache_lock&) -> cache_lock& = delete;

			~cache_lock()
			{
#if defined(Q_OS_UNIX)
				if (Descriptor >= 0) {
					static_cast<void>(::flock(Descriptor, LOCK_UN));
					static_cast<void>(::close(Descriptor));
				}
#elif defined(Q_OS_WIN)
				if (Handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CloseHandle(Handle));
				}
#endif
			}

			[[nodiscard]] auto acquire(
				const std::filesystem::path& aPath) -> bool
			{
				if (!private_directory(aPath.parent_path())) {
					return false;
				}
#if defined(Q_OS_UNIX)
				Descriptor = ::open(
					aPath.c_str(),
					O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
					S_IRUSR | S_IWUSR);
				if (Descriptor < 0) {
					return false;
				}
				struct stat information{ };
				if (
					::fstat(Descriptor, &information) != 0
					|| !S_ISREG(information.st_mode)
					|| information.st_uid != ::geteuid()
					|| information.st_nlink != 1
					|| (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) != 0
					|| ::flock(Descriptor, LOCK_EX | LOCK_NB) != 0) {
					static_cast<void>(::close(Descriptor));
					Descriptor = -1;
					return false;
				}
				Identity = exact_identity(
					information,
					exact_artifact_kind::regular_file);
				if (!Identity.has_value()) {
					static_cast<void>(::flock(Descriptor, LOCK_UN));
					static_cast<void>(::close(Descriptor));
					Descriptor = -1;
					return false;
				}
				return true;
#elif defined(Q_OS_WIN)
				Handle = CreateFileW(
					aPath.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					0U,
					nullptr,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
					nullptr);
				if (Handle == INVALID_HANDLE_VALUE) {
					return false;
				}
				FILE_ATTRIBUTE_TAG_INFO information{ };
				if (
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
						!= 0U) {
					static_cast<void>(CloseHandle(Handle));
					Handle = INVALID_HANDLE_VALUE;
					return false;
				}
				Identity = exact_identity(
					Handle,
					exact_artifact_kind::regular_file);
				if (!Identity.has_value()) {
					static_cast<void>(CloseHandle(Handle));
					Handle = INVALID_HANDLE_VALUE;
					return false;
				}
				return true;
#else
				static_cast<void>(aPath);
				return false;
#endif
			}

			[[nodiscard]] auto identity() const noexcept
				-> std::optional<exact_artifact_identity>
			{
				return Identity;
			}

			[[nodiscard]] auto manifest_identity() const
				-> std::optional<exact_artifact_identity>
			{
#if defined(Q_OS_MACOS)
				// st_dev names a mount, not a durable volume. Keep it for live
				// descriptor checks; persist the volume UUID and lock inode.
				if (!Identity || exact_identity(Descriptor, exact_artifact_kind::regular_file) != Identity) {
					return std::nullopt;
				}
				auto attributes        = attrlist{ };
				attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
				attributes.volattr     = ATTR_VOL_INFO | ATTR_VOL_UUID;
				struct volume_attributes
				{
					std::uint32_t                 Length = 0;
					std::array<std::uint8_t, 16U> Uuid{ };
				} volume;
				static_assert(sizeof(volume) == 20U);
				if (::fgetattrlist(Descriptor, &attributes, &volume, sizeof(volume), 0) != 0
					|| volume.Length != sizeof(volume)
					|| std::ranges::all_of(volume.Uuid, [](const auto aByte) { return aByte == 0U; })
					|| exact_identity(Descriptor, exact_artifact_kind::regular_file) != Identity) {
					return std::nullopt;
				}
				return exact_artifact_identity{
					.Kind       = exact_artifact_kind::regular_file,
					.Volume     = *read_u64(volume.Uuid, 0U),
					.ObjectHigh = *read_u64(volume.Uuid, 8U),
					.ObjectLow  = Identity->ObjectLow
				};
#else
				return Identity;
#endif
			}

			[[nodiscard]] auto still_names(
				const std::filesystem::path& aPath) const -> bool
			{
				return Identity.has_value()
					&& exact_identity(
						   aPath,
						   exact_artifact_kind::regular_file)
					== Identity
					&& private_regular_file(aPath);
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
			const core::git_hash_algorithm aAlgorithm)
			-> std::optional<application::git_hash_algorithm>
		{
			if (aAlgorithm == core::git_hash_algorithm::sha1) {
				return application::git_hash_algorithm::sha1;
			}
			if (aAlgorithm == core::git_hash_algorithm::sha256) {
				return application::git_hash_algorithm::sha256;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto process_operation(
			const core::epoch_operation_id& aOperation)
			-> application::git_operation_identity
		{
			return { .Bytes = aOperation.bytes() };
		}

		[[nodiscard]] auto process_scope(
			const core::pull_request_authority_scope_id& aScope)
			-> application::git_pull_request_scope_identity
		{
			return { .Bytes = aScope.bytes() };
		}

		[[nodiscard]] auto process_pack(
			const application::git_pack_oid& aPack)
			-> std::optional<application::git_pack_id>
		{
			const auto algorithm = process_algorithm(aPack.Algorithm);
			return algorithm.has_value()
				? std::optional<application::git_pack_id>{
					  application::git_pack_id{
						  .Algorithm = *algorithm,
						  .Digest    = aPack.Digest }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto process_commit(
			const core::git_commit_oid& aCommit)
			-> std::optional<application::git_commit_id>
		{
			const auto algorithm = process_algorithm(aCommit.algorithm());
			return algorithm.has_value() && aCommit.valid()
				? std::optional<application::git_commit_id>{
					  application::git_commit_id{
						  .Algorithm = *algorithm,
						  .Digest    = aCommit.bytes() }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto process_blob(
			const core::git_blob_oid& aBlob)
			-> std::optional<application::git_blob_id>
		{
			const auto algorithm = process_algorithm(aBlob.algorithm());
			return algorithm.has_value() && aBlob.valid()
				? std::optional<application::git_blob_id>{
					  application::git_blob_id{
						  .Algorithm = *algorithm,
						  .Digest    = aBlob.bytes() }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto core_commit(
			const application::git_commit_id& aCommit)
			-> core::git_commit_oid
		{
			const auto algorithm = aCommit.Algorithm == application::git_hash_algorithm::sha1
				? core::git_hash_algorithm::sha1
				: core::git_hash_algorithm::sha256;
			return core::git_commit_oid{ algorithm, aCommit.Digest };
		}

		[[nodiscard]] auto core_tree(
			const application::git_tree_id& aTree)
			-> core::git_tree_oid
		{
			const auto algorithm = aTree.Algorithm == application::git_hash_algorithm::sha1
				? core::git_hash_algorithm::sha1
				: core::git_hash_algorithm::sha256;
			return core::git_tree_oid{ algorithm, aTree.Digest };
		}

		[[nodiscard]] auto core_blob(
			const application::git_blob_id& aBlob) -> core::git_blob_oid
		{
			const auto algorithm = aBlob.Algorithm == application::git_hash_algorithm::sha1
				? core::git_hash_algorithm::sha1
				: core::git_hash_algorithm::sha256;
			return core::git_blob_oid{ algorithm, aBlob.Digest };
		}

		[[nodiscard]] auto staging_ref(
			const core::epoch_operation_id&             aOperation,
			const application::git_private_ref_endpoint aEndpoint)
			-> application::git_private_ref
		{
			return application::git_staging_ref{
				.Operation = process_operation(aOperation),
				.Endpoint  = aEndpoint,
			};
		}

		[[nodiscard]] auto retained_ref(
			const core::pull_request_authority_scope_id& aScope,
			const core::epoch_ordinal                    aOrdinal,
			const application::git_private_ref_endpoint  aEndpoint)
			-> application::git_private_ref
		{
			return application::git_epoch_ref{
				.Scope    = process_scope(aScope),
				.Ordinal  = static_cast<std::uint64_t>(aOrdinal.value()),
				.Endpoint = aEndpoint,
			};
		}

		struct inventory_snapshot
		{
			std::vector<application::git_pack_inventory_entry> Packs;
			std::uint64_t                                      RepositoryBytes = 0U;
			std::uint32_t                                      PrivateRefs     = 0U;
		};

		[[nodiscard]] auto same_inventory(
			const inventory_snapshot& aLeft,
			const inventory_snapshot& aRight) noexcept -> bool
		{
			return aLeft.Packs == aRight.Packs
				&& aLeft.RepositoryBytes == aRight.RepositoryBytes
				&& aLeft.PrivateRefs == aRight.PrivateRefs;
		}

		class cache_state;

		struct receipt_record
		{
			application::completed_git_pack_receipt Receipt;
			std::filesystem::path                   Spool;
			exact_artifact_identity                 SpoolIdentity;
			std::uint64_t                           MaximumIndexBytes = 0U;
		};

		struct spool_record
		{
			core::epoch_operation_id Operation;
			std::filesystem::path    Receiving;
			std::filesystem::path    Completed;
			exact_artifact_identity  Identity;
		};

		struct observation_record
		{
			application::git_preimport_observation  Observation;
			application::completed_git_pack_receipt Receipt;
			std::uint64_t                           MaximumIndexBytes = 0U;
		};

		class cache_state final
			: public std::enable_shared_from_this<cache_state>
		{
		public:
			cache_state(
				application::git_cache_open_request            aRequest,
				std::filesystem::path                          aStorageRoot,
				std::filesystem::path                          aRepository,
				std::optional<exact_artifact_identity>         aRepositoryIdentity,
				std::filesystem::path                          aLockPath,
				exact_artifact_identity                        aLockIdentity,
				exact_artifact_identity                        aManifestLockIdentity,
				std::unique_ptr<cache_lock>                    aLock,
				std::unique_ptr<application::git_process_port> aProcess)
				: Request{ std::move(aRequest) }
				, StorageRoot{ std::move(aStorageRoot) }
				, Repository{ std::move(aRepository) }
				, RepositoryIdentity{ aRepositoryIdentity }
				, LockPath{ std::move(aLockPath) }
				, LockIdentity{ aLockIdentity }
				, ManifestLockIdentity{ aManifestLockIdentity }
				, Lock{ std::move(aLock) }
				, Process{ std::move(aProcess) }
			{
			}

			[[nodiscard]] auto bind_repository(
				std::filesystem::path          aRepository,
				const exact_artifact_identity& aIdentity) -> bool
			{
				if (
					aIdentity.Kind != exact_artifact_kind::directory
					|| !private_directory(aRepository)
					|| exact_identity(aRepository, aIdentity.Kind) != aIdentity) {
					return false;
				}
				Repository         = std::move(aRepository);
				RepositoryIdentity = aIdentity;
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

			[[nodiscard]] auto validate_topology_without_manifest(
				const std::stop_token aStop = { }) const -> bool
			{
				if (
					aStop.stop_requested()
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
						"ref: refs/heads/main\n")) {
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

			[[nodiscard]] auto expected_manifest() const -> std::vector<std::uint8_t>
			{
#if defined(Q_OS_MACOS)
				return manifest_bytes(Request, ManifestLockIdentity, true);
#else
				return manifest_bytes(Request, ManifestLockIdentity, false);
#endif
			}

			[[nodiscard]] auto validate_topology(const std::stop_token aStop = { }) const -> bool
			{
				if (!validate_topology_without_manifest(aStop)) {
					return false;
				}
				const auto expected = expected_manifest();
				const auto manifest = read_private_file(Repository / manifest_name, expected.size());
				// A legacy populated cache may still be used under its original
				// exact mount identity. Never reinterpret its objects after drift.
				return manifest && (*manifest == expected || *manifest == manifest_bytes(Request, LockIdentity, false))
					&& active_boundary_still_owned();
			}

			[[nodiscard]] auto pristine_legacy_cache_after_remount(const std::stop_token aStop) const -> bool
			{
#if defined(Q_OS_MACOS)
				if (!validate_topology_without_manifest(aStop)) {
					return false;
				}
				const auto authority = manifest_authority_bytes(Request);
				const auto manifest  = read_private_file(Repository / manifest_name, authority.size() + 24U);
				if (!manifest || manifest->size() != authority.size() + 24U
					|| !std::equal(authority.begin(), authority.end(), manifest->begin())) {
					return false;
				}
				const auto prior = read_identity(*manifest, authority.size(), exact_artifact_kind::regular_file);
				if (!prior || prior->Volume == 0U || prior->Volume == LockIdentity.Volume
					|| prior->ObjectHigh != 0U || prior->ObjectLow != LockIdentity.ObjectLow) {
					return false;
				}
				// Only the closed, freshly initialized layout is recoverable.
				// No object, ref, worktree, unknown entry or link may be adopted.
				std::error_code error;
				auto            count = std::size_t{ 0 };
				for (auto entry = std::filesystem::recursive_directory_iterator{ Repository, error };
					 !error && entry != std::filesystem::recursive_directory_iterator{ }; entry.increment(error)) {
					if (aStop.stop_requested() || ++count > 9U) {
						return false;
					}
					const auto name      = entry->path().lexically_relative(Repository).generic_string();
					const auto directory = name == "objects" || name == "objects/info" || name == "objects/pack"
						|| name == "refs" || name == "refs/heads" || name == "refs/tags";
					const auto file = name == "HEAD" || name == "config" || name == manifest_name;
					if ((!directory && !file)
						|| (directory && !private_directory(entry->path()))
						|| (file && !private_regular_file(entry->path()))) {
						return false;
					}
				}
				return !error && active_boundary_still_owned();
#else
				static_cast<void>(aStop);
				return false;
#endif
			}

			[[nodiscard]] auto inventory(
				const std::stop_token aStop          = { },
				const bool            aEnforceLimits = true) const
				-> application::git_cache_result<inventory_snapshot>
			{
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto      repositoryIdentity = *RepositoryIdentity;
				auto            snapshot           = inventory_snapshot{ };
				std::error_code error;
				auto            entryCount = std::size_t{ 0U };
				for (
					auto iterator = std::filesystem::recursive_directory_iterator{
						Repository,
						std::filesystem::directory_options::none,
						error };
					iterator != std::filesystem::recursive_directory_iterator{ }; iterator.increment(error)) {
					if (aStop.stop_requested()) {
						return std::unexpected{
							application::git_cache_failure::cancelled
						};
					}
					if (error || ++entryCount > maximum_repository_entries) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto path   = iterator->path();
					const auto status = std::filesystem::symlink_status(path, error);
					if (error || std::filesystem::is_symlink(status)) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					if (
						status.type()
						== std::filesystem::file_type::directory) {
						const auto identity = exact_identity(
							path,
							exact_artifact_kind::directory);
						if (
							!identity.has_value()
							|| identity->Volume
								!= repositoryIdentity.Volume
							|| !private_directory(path)) {
							return std::unexpected{
								application::git_cache_failure::
									invalid_repository
							};
						}
						continue;
					}
					if (
						status.type()
							!= std::filesystem::file_type::regular
						|| !private_regular_file(path)) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto identity = exact_identity(
						path,
						exact_artifact_kind::regular_file);
					if (
						!identity.has_value()
						|| identity->Volume
							!= repositoryIdentity.Volume) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto size = std::filesystem::file_size(path, error);
					if (
						error
						|| size
							> std::numeric_limits<std::uint64_t>::max()
								- snapshot.RepositoryBytes) {
						return std::unexpected{
							application::git_cache_failure::
								quota_exceeded
						};
					}
					snapshot.RepositoryBytes += size;
					const auto relative = path.lexically_relative(Repository).generic_string();
					if (relative.starts_with("refs/betterpullreview/")) {
						if (
							snapshot.PrivateRefs
							== std::numeric_limits<std::uint32_t>::max()) {
							return std::unexpected{
								application::git_cache_failure::
									quota_exceeded
							};
						}
						++snapshot.PrivateRefs;
					}
				}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				std::error_code recursiveScanInjectionError;
				if (
					!error
					&& std::filesystem::exists(
						StorageRoot
							/ "inject-cache-recursive-scan-terminal-error",
						recursiveScanInjectionError)
					&& !recursiveScanInjectionError) {
					error = std::make_error_code(std::errc::io_error);
				} else if (recursiveScanInjectionError) {
					error = recursiveScanInjectionError;
				}
#endif
				if (error) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				if (
					aEnforceLimits
					&& (snapshot.RepositoryBytes
							> Request.Limits.RepositoryBytes
						|| snapshot.PrivateRefs
							> Request.Limits.PrivateRefCount)) {
					return std::unexpected{
						application::git_cache_failure::quota_exceeded
					};
				}

				const auto packDirectory = Repository / "objects" / "pack";
				if (missing(packDirectory)) {
					return active_boundary_still_owned()
						? application::git_cache_result<
							  inventory_snapshot>{ std::move(snapshot) }
						: application::git_cache_result<
							  inventory_snapshot>{
							  std::unexpected{
								  application::git_cache_failure::
									  invalid_repository }
						  };
				}
				if (!private_directory(packDirectory)) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				struct pair_sizes
				{
					application::git_pack_oid    Pack;
					std::optional<std::uint64_t> PackBytes;
					std::optional<std::uint64_t> IndexBytes;
				};
				auto pairs        = std::map<std::string, pair_sizes>{ };
				auto packIterator = std::filesystem::directory_iterator{
					packDirectory,
					std::filesystem::directory_options::none,
					error
				};
				const auto packEnd = std::filesystem::directory_iterator{ };
				while (!error && packIterator != packEnd) {
					const auto entry = *packIterator;
					if (aStop.stop_requested()) {
						return std::unexpected{
							application::git_cache_failure::cancelled
						};
					}
					if (error || !private_regular_file(entry.path())) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto name             = entry.path().filename().string();
					const auto digestCharacters = Request.ObjectFormat
							== core::git_hash_algorithm::sha1
						? 40U
						: 64U;
					const auto suffix           = name.ends_with(".pack")
						? std::string_view{ ".pack" }
						: name.ends_with(".idx")
						? std::string_view{ ".idx" }
						: std::string_view{ };
					if (
						suffix.empty()
						|| name.size()
							!= 5U + digestCharacters + suffix.size()
						|| !name.starts_with("pack-")) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto digest = std::string_view{ name }.substr(5U, digestCharacters);
					const auto parsed = parse_pack_oid(digest, Request.ObjectFormat);
					if (!parsed.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto size = std::filesystem::file_size(entry.path(), error);
					if (error || size == 0U) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					auto& pair        = pairs[std::string{ digest }];
					pair.Pack         = *parsed;
					auto& destination = suffix == ".pack" ? pair.PackBytes : pair.IndexBytes;
					if (destination.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					destination = size;
					packIterator.increment(error);
				}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				std::error_code scanInjectionError;
				if (
					!error
					&& std::filesystem::exists(
						StorageRoot
							/ "inject-cache-pack-scan-terminal-error",
						scanInjectionError)
					&& !scanInjectionError) {
					error = std::make_error_code(std::errc::io_error);
				} else if (scanInjectionError) {
					error = scanInjectionError;
				}
#endif
				if (error) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				if (aEnforceLimits && pairs.size() > Request.Limits.PackCount) {
					return std::unexpected{
						application::git_cache_failure::quota_exceeded
					};
				}
				snapshot.Packs.reserve(pairs.size());
				for (const auto& [digest, pair] : pairs) {
					static_cast<void>(digest);
					if (
						!pair.PackBytes.has_value()
						|| !pair.IndexBytes.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					snapshot.Packs.push_back({
						.Pack       = pair.Pack,
						.PackBytes  = *pair.PackBytes,
						.IndexBytes = *pair.IndexBytes,
					});
				}
				return active_boundary_still_owned()
					? application::git_cache_result<
						  inventory_snapshot>{ std::move(snapshot) }
					: application::git_cache_result<
						  inventory_snapshot>{
						  std::unexpected{
							  application::git_cache_failure::
								  invalid_repository }
					  };
			}

			[[nodiscard]] auto validated_inventory(
				const std::stop_token aStop          = { },
				const bool            aEnforceLimits = true) const
				-> application::git_cache_result<inventory_snapshot>
			{
				if (!validate_topology(aStop)) {
					return std::unexpected{
						aStop.stop_requested()
							? application::git_cache_failure::cancelled
							: application::git_cache_failure::
								  invalid_repository
					};
				}
				return inventory(aStop, aEnforceLimits);
			}

			[[nodiscard]] auto admit_incoming_file(const std::stop_token aStop) const -> application::git_cache_result<void>
			{
				// Includes preserved unowned empties and unrelated entries. Do not
				// recurse or delete anything to make room; stop at the fixed admission
				// limit even if the directory is larger than the cache expects.
				std::error_code error;
				auto            entry = std::filesystem::directory_iterator(StorageRoot / "incoming" / "v1", error);
				std::uint32_t   count = 0U;
				for (; !error && entry != std::filesystem::directory_iterator{ }; entry.increment(error)) {
					if (aStop.stop_requested()) {
						return std::unexpected{ application::git_cache_failure::cancelled };
					}
					if (++count >= application::maximum_git_cache_pack_count) {
						return std::unexpected{ application::git_cache_failure::quota_exceeded };
					}
				}
				if (error) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				return { };
			}

			[[nodiscard]] auto process_failure(
				const application::git_process_error& aError)
				-> application::git_cache_error
			{
				auto failure = application::git_cache_failure::process_failure;
				if (aError.Code == application::git_process_failure::reap_failure) {
					retain_after_reap_failure();
					failure = application::git_cache_failure::repair_required;
				} else if (
					aError.Code == application::git_process_failure::cancelled) {
					failure = application::git_cache_failure::cancelled;
				} else if (
					aError.Code
					== application::git_process_failure::invalid_repository) {
					failure = application::git_cache_failure::invalid_repository;
				} else if (
					aError.Code
					== application::git_process_failure::unsupported_object) {
					// [[BPR-0880]] The fsck message class travels with it.
					failure = application::git_cache_failure::unsupported_object;
				}
				return { failure, aError.Detail };
			}

			auto retain_after_reap_failure() -> void
			{
				if (!Retained.exchange(true)) {
					static_cast<void>(
						new std::shared_ptr<cache_state>{
							shared_from_this() });
				}
			}

			[[nodiscard]] auto new_token()
				-> std::array<std::uint8_t, 32U>
			{
				auto token     = std::array<std::uint8_t, 32U>{ };
				auto generator = QRandomGenerator::securelySeeded();
				do {
					for (std::size_t index = 0U; index < token.size(); index += 4U) {
						const auto value  = generator.generate();
						token[index]      = static_cast<std::uint8_t>(value >> 24U);
						token[index + 1U] = static_cast<std::uint8_t>(value >> 16U);
						token[index + 2U] = static_cast<std::uint8_t>(value >> 8U);
						token[index + 3U] = static_cast<std::uint8_t>(value);
					}
				} while (!application::valid_cache_token(token));
				return token;
			}

			auto register_receipt(receipt_record aRecord) -> bool
			{
				std::lock_guard guard{ RecordsMutex };
				if (
					LifecycleExhausted
					|| !application::agrees(aRecord.Receipt)
					|| exact_identity(
						   aRecord.Receipt.Spool.Identity)
						!= aRecord.SpoolIdentity
					|| std::ranges::find(
						   AbortedOperations,
						   aRecord.Receipt.Operation)
						!= AbortedOperations.end()) {
					return false;
				}
				const auto spool = std::ranges::find_if(
					Spools,
					[&](const auto& aExisting) {
						return aExisting.Operation
							== aRecord.Receipt.Operation
							&& aExisting.Completed == aRecord.Spool
							&& aExisting.Identity == aRecord.SpoolIdentity;
					});
				const auto duplicate = std::ranges::any_of(
					Receipts,
					[&](const auto& aExisting) {
						return aExisting.Receipt.Operation
							== aRecord.Receipt.Operation;
					});
				if (
					spool == Spools.end() || duplicate
					|| Receipts.size() >= Request.Limits.PackCount) {
					return false;
				}
				Receipts.push_back(std::move(aRecord));
				return true;
			}

			auto register_spool(spool_record aRecord) -> bool
			{
				std::lock_guard guard{ RecordsMutex };
				if (
					Spools.size()
						>= application::maximum_git_cache_pack_count
					|| LifecycleExhausted
					|| std::ranges::find(
						   AbortedOperations,
						   aRecord.Operation)
						!= AbortedOperations.end()
					|| std::ranges::any_of(
						Spools,
						[&](const auto& aExisting) {
							return aExisting.Operation == aRecord.Operation;
						})) {
					return false;
				}
				Spools.push_back(std::move(aRecord));
				return true;
			}

			[[nodiscard]] auto operation_aborted(
				const core::epoch_operation_id& aOperation)
				-> bool
			{
				std::lock_guard guard{ RecordsMutex };
				return LifecycleExhausted
					|| std::ranges::find(AbortedOperations, aOperation)
					!= AbortedOperations.end();
			}

			[[nodiscard]] auto mark_operation_aborted(
				const core::epoch_operation_id& aOperation) -> bool
			{
				std::lock_guard guard{ RecordsMutex };
				if (
					std::ranges::find(AbortedOperations, aOperation)
					!= AbortedOperations.end()) {
					return true;
				}
				if (
					AbortedOperations.size()
					>= application::maximum_git_cache_pack_count) {
					LifecycleExhausted = true;
					return true;
				}
				AbortedOperations.push_back(aOperation);
				return true;
			}

			[[nodiscard]] auto find_spool(
				const core::epoch_operation_id& aOperation)
				-> std::optional<spool_record>
			{
				std::lock_guard guard{ RecordsMutex };
				const auto      found = std::ranges::find_if(
					Spools,
					[&](const auto& aRecord) {
						return aRecord.Operation == aOperation;
					});
				return found == Spools.end()
					? std::nullopt
					: std::optional<spool_record>{ *found };
			}

			[[nodiscard]] auto find_receipt(
				const application::completed_git_pack_receipt& aReceipt)
				-> std::optional<receipt_record>
			{
				std::lock_guard guard{ RecordsMutex };
				const auto      found = std::ranges::find_if(
					Receipts,
					[&](const auto& aRecord) {
						return aRecord.Receipt == aReceipt;
					});
				return found == Receipts.end()
					? std::nullopt
					: std::optional<receipt_record>{ *found };
			}

			auto register_observation(observation_record aRecord) -> bool
			{
				std::lock_guard guard{ RecordsMutex };
				const auto      found = std::ranges::find_if(
					Observations,
					[&](const auto& aExisting) {
						return aExisting.Observation.Operation
							== aRecord.Observation.Operation;
					});
				if (found != Observations.end()) {
					*found = std::move(aRecord);
					return true;
				}
				if (Observations.size() >= Request.Limits.PackCount) {
					return false;
				}
				Observations.push_back(std::move(aRecord));
				return true;
			}

			[[nodiscard]] auto find_observation(
				const application::git_preimport_observation&  aObservation,
				const application::completed_git_pack_receipt& aReceipt)
				-> std::optional<observation_record>
			{
				std::lock_guard guard{ RecordsMutex };
				const auto      found = std::ranges::find_if(
					Observations,
					[&](const auto& aRecord) {
						return aRecord.Observation == aObservation
							&& aRecord.Receipt == aReceipt;
					});
				return found == Observations.end()
					? std::nullopt
					: std::optional<observation_record>{ *found };
			}

			auto forget_operation(
				const core::epoch_operation_id& aOperation) -> void
			{
				std::lock_guard guard{ RecordsMutex };
				std::erase_if(
					Receipts,
					[&](const auto& aRecord) {
						return aRecord.Receipt.Operation == aOperation;
					});
				std::erase_if(
					Observations,
					[&](const auto& aRecord) {
						return aRecord.Observation.Operation == aOperation;
					});
				std::erase_if(
					Spools,
					[&](const auto& aRecord) {
						return aRecord.Operation == aOperation;
					});
			}

			application::git_cache_open_request            Request;
			std::filesystem::path                          StorageRoot;
			std::filesystem::path                          Repository;
			std::optional<exact_artifact_identity>         RepositoryIdentity;
			std::filesystem::path                          LockPath;
			exact_artifact_identity                        LockIdentity;
			exact_artifact_identity                        ManifestLockIdentity;
			std::unique_ptr<cache_lock>                    Lock;
			std::unique_ptr<application::git_process_port> Process;
			std::recursive_mutex                           OperationMutex;

		private:
			std::mutex                            RecordsMutex;
			std::vector<spool_record>             Spools;
			std::vector<core::epoch_operation_id> AbortedOperations;
			bool                                  LifecycleExhausted = false;
			std::vector<receipt_record>           Receipts;
			std::vector<observation_record>       Observations;
			std::atomic_bool                      Retained = false;
		};

		class private_pack_file final
		{
		public:
			private_pack_file()                         = default;
			private_pack_file(const private_pack_file&) = delete;
			auto operator=(const private_pack_file&)
				-> private_pack_file& = delete;

			~private_pack_file()
			{
				close();
			}

			[[nodiscard]] auto create(
				const std::filesystem::path& aPath) -> bool
			{
				Path = aPath;
#if defined(Q_OS_UNIX)
				Descriptor = ::open(
					aPath.c_str(),
					O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
					S_IRUSR | S_IWUSR);
				return Descriptor >= 0;
#elif defined(Q_OS_WIN)
				// FILE_SHARE_DELETE and nothing else: the receiving spool
				// lives in an owner-private directory, so only this user can
				// reach it, and an abort while the sink is still open renames
				// and unlinks the file — legal on Windows only when the open
				// handle shares deletion, which is what POSIX gives an open
				// descriptor for free (BPR-0437).
				Handle = CreateFileW(
					aPath.c_str(),
					GENERIC_WRITE,
					FILE_SHARE_DELETE,
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
				const std::span<const std::byte> aBytes) -> bool
			{
				auto offset = std::size_t{ 0U };
				while (offset < aBytes.size()) {
#if defined(Q_OS_UNIX)
					const auto written = ::write(
						Descriptor,
						aBytes.data() + offset,
						aBytes.size() - offset);
					if (written < 0 && errno == EINTR) {
						continue;
					}
					if (written <= 0) {
						return false;
					}
					offset += static_cast<std::size_t>(written);
#elif defined(Q_OS_WIN)
					const auto requested = static_cast<DWORD>(
						std::min<std::size_t>(
							aBytes.size() - offset,
							std::numeric_limits<DWORD>::max()));
					DWORD written = 0U;
					if (
						WriteFile(
							Handle,
							aBytes.data() + offset,
							requested,
							&written,
							nullptr)
							== 0
						|| written == 0U) {
						return false;
					}
					offset += written;
#else
					static_cast<void>(aBytes);
					return false;
#endif
				}
				return true;
			}

			[[nodiscard]] auto synchronize_and_close() -> bool
			{
#if defined(Q_OS_UNIX)
				if (Descriptor < 0) {
					return false;
				}
				const auto synchronized = ::fsync(Descriptor) == 0;
				const auto closed       = ::close(Descriptor) == 0;
				Descriptor              = -1;
				return synchronized && closed;
#elif defined(Q_OS_WIN)
				if (Handle == INVALID_HANDLE_VALUE) {
					return false;
				}
				const auto synchronized = FlushFileBuffers(Handle) != 0;
				const auto closed       = CloseHandle(Handle) != 0;
				Handle                  = INVALID_HANDLE_VALUE;
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
				if (Descriptor >= 0) {
					static_cast<void>(::close(Descriptor));
					Descriptor = -1;
				}
#elif defined(Q_OS_WIN)
				if (Handle != INVALID_HANDLE_VALUE) {
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
			const std::filesystem::path&   aPath,
			const exact_artifact_identity& aExpected) -> bool;
		[[nodiscard]] auto synchronize_directory(
			const std::filesystem::path& aPath) -> bool;
		[[nodiscard]] auto rename_no_replace(
			const std::filesystem::path& aSource,
			const std::filesystem::path& aDestination,
			std::error_code&             aError) noexcept -> bool;

#if defined(Q_OS_WIN)
		/// \brief `MoveFileExW` without replacement, repeated while another
		/// handle briefly holds the file ([[BPR-1040]], `retry_while_held`).
		[[nodiscard]] auto move_file_no_replace(
			const std::filesystem::path& aSource,
			const std::filesystem::path& aDestination) -> DWORD
		{
			return platform::retry_while_held(platform::held_errors::sharing_lock_and_access_denied, [&]() -> DWORD {
				return MoveFileExW(aSource.c_str(), aDestination.c_str(), MOVEFILE_WRITE_THROUGH) != 0
					? ERROR_SUCCESS
					: GetLastError();
			});
		}
#endif

		/// \brief Removes one file name; on Windows, repeated while another
		/// handle briefly holds the file ([[BPR-1104]], `retry_while_held`).
		///
		/// \details `remove_exact_private_file` renames a spool to a quarantine
		/// name and then deletes it. [[BPR-1040]] retried the rename; on the
		/// Windows station the delete right after it then failed with
		/// `ERROR_SHARING_VIOLATION`, and the same delete succeeded 50 to 250 ms
		/// later. The delete now takes the same bounded retry, and after it the
		/// last error fails the removal as before. A name that is not there is
		/// not removed, as before.
		[[nodiscard]] auto remove_file_name(const std::filesystem::path& aPath) -> bool
		{
#if defined(Q_OS_WIN)
			const auto removed = platform::retry_while_held(platform::held_errors::sharing_lock_and_access_denied, [&]() -> DWORD {
				std::error_code error;
				if (std::filesystem::remove(aPath, error) && !error) {
					return ERROR_SUCCESS;
				}
				if (!error) {
					return ERROR_FILE_NOT_FOUND;
				}
				return error.category() == std::system_category()
					? static_cast<DWORD>(error.value())
					: ERROR_GEN_FAILURE;
			});
			return removed == ERROR_SUCCESS;
#else
			std::error_code error;
			return std::filesystem::remove(aPath, error) && !error;
#endif
		}

		struct spool_publish_result
		{
			bool Success        = false;
			bool CompletedOwned = false;
		};

		[[nodiscard]] auto publish_spool(
			const std::filesystem::path& aReceiving,
			const std::filesystem::path& aCompleted) -> spool_publish_result
		{
			if (!missing(aCompleted) || !private_regular_file(aReceiving)) {
				return { };
			}
#if defined(Q_OS_UNIX)
			std::error_code renameError;
			if (!rename_no_replace(aReceiving, aCompleted, renameError)) {
				return { };
			}
			const auto directory = ::open(
				aCompleted.parent_path().c_str(),
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
			if (directory < 0) {
				return {
					.Success        = false,
					.CompletedOwned = true,
				};
			}
			const auto synchronized = ::fsync(directory) == 0;
			static_cast<void>(::close(directory));
			return {
				.Success        = synchronized && private_regular_file(aCompleted),
				.CompletedOwned = true,
			};
#elif defined(Q_OS_WIN)
			if (move_file_no_replace(aReceiving, aCompleted) != ERROR_SUCCESS) {
				return { };
			}
			return {
				.Success        = private_regular_file(aCompleted),
				.CompletedOwned = true,
			};
#else
			static_cast<void>(aReceiving);
			static_cast<void>(aCompleted);
			return { };
#endif
		}

		class pack_sink final : public application::git_pack_stream_sink
		{
		public:
			pack_sink(
				std::shared_ptr<cache_state>   aState,
				core::epoch_operation_id       aOperation,
				std::filesystem::path          aReceiving,
				std::filesystem::path          aCompleted,
				const std::uint64_t            aMaximumBytes,
				const core::git_hash_algorithm aAlgorithm)
				: State{ std::move(aState) }
				, Operation{ aOperation }
				, Receiving{ std::move(aReceiving) }
				, Completed{ std::move(aCompleted) }
				, MaximumBytes{ aMaximumBytes }
				, Algorithm{ aAlgorithm }
				, DigestBytes{
					aAlgorithm == core::git_hash_algorithm::sha1 ? 20U : 32U
				}
				, Hash{ aAlgorithm == core::git_hash_algorithm::sha1 ? QCryptographicHash::Sha1 : QCryptographicHash::Sha256 }
			{
			}

			~pack_sink() override
			{
				abort();
			}

			[[nodiscard]] auto initialize(const std::function<bool(const application::git_spool_ownership&)>& aWitness) -> bool
			{
				Tail.reserve(DigestBytes);
				Header.reserve(12U);
				if (!File.create(Receiving)) {
					return false;
				}
				ReceivingIdentity = File.identity();
				Registered        = ReceivingIdentity.has_value()
					&& State->register_spool({
						.Operation = Operation,
						.Receiving = Receiving,
						.Completed = Completed,
						.Identity  = *ReceivingIdentity,
					});
				if (!Registered) {
					return false;
				}
				const auto identity = artifact_identity(*ReceivingIdentity);
				return identity && (!aWitness || aWitness({ .Operation = Operation, .Identity = *identity, .MaximumBytes = MaximumBytes }));
			}

			[[nodiscard]] auto append(
				const std::span<const std::byte> aBytes,
				const std::stop_token            aStop)
				-> application::git_pack_stream_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (
					Finished || Aborted
					|| State->operation_aborted(Operation)
					|| aBytes.empty()) {
					return std::unexpected{
						application::git_pack_stream_failure::
							invalid_state
					};
				}
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_pack_stream_failure::cancelled
					};
				}
				if (
					Bytes > MaximumBytes
					|| aBytes.size() > MaximumBytes - Bytes) {
					return std::unexpected{
						application::git_pack_stream_failure::input_limit
					};
				}
				const auto headerNeeded = std::min<std::size_t>(
					12U - Header.size(),
					aBytes.size());
				for (std::size_t index = 0U; index < headerNeeded; ++index) {
					Header.push_back(
						static_cast<std::uint8_t>(aBytes[index]));
				}
				if (!File.write(aBytes)) {
					return std::unexpected{
						application::git_pack_stream_failure::storage
					};
				}

				const auto* begin = reinterpret_cast<const char*>(aBytes.data());
				const auto  total = Tail.size() + aBytes.size();
				if (total <= DigestBytes) {
					Tail.insert(Tail.end(), aBytes.begin(), aBytes.end());
				} else {
					const auto hashBytes = total - DigestBytes;
					const auto fromTail  = std::min<std::size_t>(hashBytes, Tail.size());
					if (fromTail != 0U) {
						Hash.addData(QByteArrayView{
							reinterpret_cast<const char*>(Tail.data()),
							static_cast<qsizetype>(fromTail) });
					}
					const auto fromInput = hashBytes - fromTail;
					if (fromInput != 0U) {
						Hash.addData(QByteArrayView{
							begin,
							static_cast<qsizetype>(fromInput) });
					}
					auto replacement = std::vector<std::byte>{ };
					replacement.reserve(DigestBytes);
					if (fromTail < Tail.size()) {
						replacement.insert(
							replacement.end(),
							Tail.begin()
								+ static_cast<std::ptrdiff_t>(fromTail),
							Tail.end());
					}
					replacement.insert(
						replacement.end(),
						aBytes.begin()
							+ static_cast<std::ptrdiff_t>(fromInput),
						aBytes.end());
					Tail = std::move(replacement);
				}
				Bytes += aBytes.size();
				return { };
			}

			[[nodiscard]] auto finish(const std::stop_token aStop)
				-> application::git_pack_stream_result<
					application::completed_git_pack_receipt> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (
					Finished || Aborted
					|| State->operation_aborted(Operation)) {
					return std::unexpected{
						application::git_pack_stream_failure::
							invalid_state
					};
				}
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_pack_stream_failure::cancelled
					};
				}
				if (
					Bytes <= 12U + DigestBytes || Header.size() != 12U
					|| Header[0] != 0x50U || Header[1] != 0x41U
					|| Header[2] != 0x43U || Header[3] != 0x4bU
					|| Header[4] != 0U || Header[5] != 0U
					|| Header[6] != 0U
					|| (Header[7] != 2U && Header[7] != 3U)
					|| Tail.size() != DigestBytes) {
					return std::unexpected{
						application::git_pack_stream_failure::
							malformed_pack
					};
				}
				const auto observedBytes = Hash.result();
				if (
					static_cast<std::size_t>(observedBytes.size())
						!= DigestBytes
					|| !std::equal(
						Tail.begin(),
						Tail.end(),
						reinterpret_cast<const std::byte*>(
							observedBytes.constData()))) {
					return std::unexpected{
						application::git_pack_stream_failure::
							malformed_pack
					};
				}
				if (!File.synchronize_and_close()) {
					return std::unexpected{
						application::git_pack_stream_failure::storage
					};
				}
				const auto maximumIndex = maximum_index_bytes(Algorithm, Header);
				if (!maximumIndex.has_value()) {
					return std::unexpected{
						application::git_pack_stream_failure::input_limit
					};
				}
				const auto publication = publish_spool(Receiving, Completed);
				CompletedOwned         = publication.CompletedOwned;
				if (
					!publication.Success || !ReceivingIdentity.has_value()
					|| exact_identity(
						   Completed,
						   exact_artifact_kind::regular_file)
						!= ReceivingIdentity) {
					return std::unexpected{
						application::git_pack_stream_failure::storage
					};
				}
				auto pack = application::git_pack_oid{
					.Algorithm = Algorithm
				};
				std::transform(
					Tail.begin(),
					Tail.end(),
					pack.Digest.begin(),
					[](const std::byte aValue) {
						return static_cast<std::uint8_t>(aValue);
					});
				const auto durableSpoolIdentity = artifact_identity(*ReceivingIdentity);
				if (!durableSpoolIdentity.has_value()) {
					return std::unexpected{
						application::git_pack_stream_failure::storage
					};
				}
				auto receipt = application::completed_git_pack_receipt{
					.Operation    = Operation,
					.ExpectedPack = pack,
					.ObservedPack = pack,
					.Spool        = {
						.Role = application::git_artifact_role::
							completed_spool,
						.Identity = *durableSpoolIdentity,
						.Bytes    = Bytes,
					},
					.OpaqueToken = State->new_token(),
				};
				if (
					!State->register_receipt({
						.Receipt           = receipt,
						.Spool             = Completed,
						.SpoolIdentity     = *ReceivingIdentity,
						.MaximumIndexBytes = *maximumIndex,
					})) {
					return std::unexpected{
						application::git_pack_stream_failure::
							invalid_state
					};
				}
				Finished       = true;
				CompletedOwned = false;
				return receipt;
			}

			auto abort() noexcept -> void override
			{
				try {
					std::lock_guard operationGuard{
						State->OperationMutex
					};
					if (Finished) {
						return;
					}
					if (!Aborted) {
						if (Registered) {
							static_cast<void>(
								State->mark_operation_aborted(Operation));
						}
						File.close();
						Aborted = true;
					}
					auto cleaned = missing(Receiving);
					if (ReceivingIdentity.has_value() && !cleaned) {
						cleaned = remove_exact_private_file(
							Receiving,
							*ReceivingIdentity);
					}
					if (CompletedOwned) {
						auto completedCleaned = missing(Completed);
						if (
							ReceivingIdentity.has_value()
							&& !completedCleaned) {
							completedCleaned = remove_exact_private_file(
								Completed,
								*ReceivingIdentity);
						}
						CompletedOwned = !completedCleaned;
						cleaned        = cleaned && completedCleaned;
					}
					static_cast<void>(
						synchronize_directory(
							Receiving.parent_path()));
					if (cleaned && Registered) {
						State->forget_operation(Operation);
					}
				} catch (...) {
				}
			}

		private:
			std::shared_ptr<cache_state>           State;
			core::epoch_operation_id               Operation;
			std::filesystem::path                  Receiving;
			std::filesystem::path                  Completed;
			std::uint64_t                          MaximumBytes = 0U;
			core::git_hash_algorithm               Algorithm    = core::git_hash_algorithm::invalid;
			std::size_t                            DigestBytes  = 0U;
			QCryptographicHash                     Hash;
			private_pack_file                      File;
			std::vector<std::uint8_t>              Header;
			std::vector<std::byte>                 Tail;
			std::optional<exact_artifact_identity> ReceivingIdentity;
			std::uint64_t                          Bytes          = 0U;
			bool                                   Finished       = false;
			bool                                   Aborted        = false;
			bool                                   CompletedOwned = false;
			bool                                   Registered     = false;
		};

		[[nodiscard]] auto synchronize_directory(
			const std::filesystem::path& aPath) -> bool
		{
#if defined(Q_OS_UNIX)
			const auto descriptor = ::open(
				aPath.c_str(),
				O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
			if (descriptor < 0) {
				return false;
			}
			const auto synchronized = ::fsync(descriptor) == 0;
			static_cast<void>(::close(descriptor));
			return synchronized;
#elif defined(Q_OS_WIN)
			static_cast<void>(aPath);
			return true;
#else
			static_cast<void>(aPath);
			return false;
#endif
		}

		[[nodiscard]] auto rename_no_replace(
			const std::filesystem::path& aSource,
			const std::filesystem::path& aDestination,
			std::error_code&             aError) noexcept -> bool
		{
			aError.clear();
#if defined(Q_OS_MACOS)
			if (
				::renamex_np(
					aSource.c_str(),
					aDestination.c_str(),
					RENAME_EXCL)
				== 0) {
				return true;
			}
			aError = std::error_code{ errno, std::generic_category() };
			return false;
#elif defined(Q_OS_LINUX)
			if (
				::syscall(
					SYS_renameat2,
					AT_FDCWD,
					aSource.c_str(),
					AT_FDCWD,
					aDestination.c_str(),
					RENAME_NOREPLACE)
				== 0) {
				return true;
			}
			aError = std::error_code{ errno, std::generic_category() };
			return false;
#elif defined(Q_OS_WIN)
			const auto moved = move_file_no_replace(aSource, aDestination);
			if (moved == ERROR_SUCCESS) {
				return true;
			}
			aError = std::error_code{ static_cast<int>(moved), std::system_category() };
			return false;
#else
			static_cast<void>(aSource);
			static_cast<void>(aDestination);
			aError = std::make_error_code(
				std::errc::operation_not_supported);
			return false;
#endif
		}

		[[nodiscard]] auto random_quarantine_path(
			const std::filesystem::path& aSource,
			const std::string_view       aLabel)
			-> std::optional<std::filesystem::path>
		{
			auto generator = QRandomGenerator::securelySeeded();
			for (std::size_t attempt = 0U; attempt < 8U; ++attempt) {
				auto nonce = std::array<std::uint8_t, 16U>{ };
				for (std::size_t index = 0U; index < nonce.size(); index += 4U) {
					const auto value  = generator.generate();
					nonce[index]      = static_cast<std::uint8_t>(value >> 24U);
					nonce[index + 1U] = static_cast<std::uint8_t>(value >> 16U);
					nonce[index + 2U] = static_cast<std::uint8_t>(value >> 8U);
					nonce[index + 3U] = static_cast<std::uint8_t>(value);
				}
				auto candidate = aSource.parent_path()
					/ (std::string{ aLabel } + lower_hex(nonce));
				if (missing(candidate)) {
					return candidate;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto rename_exact_to_new_path(
			const std::filesystem::path&   aSource,
			const std::filesystem::path&   aDestination,
			const exact_artifact_identity& aExpected) -> bool
		{
			const auto privateSource = aExpected.Kind == exact_artifact_kind::regular_file
				? private_regular_file(aSource)
				: private_directory(aSource);
			if (
				!privateSource
				|| exact_identity(aSource, aExpected.Kind) != aExpected
				|| !private_directory(aSource.parent_path())
				|| !private_directory(aDestination.parent_path())) {
				return false;
			}
			std::error_code error;
			if (!rename_no_replace(aSource, aDestination, error)) {
				return false;
			}
			return missing(aSource)
				&& exact_identity(aDestination, aExpected.Kind) == aExpected
				&& synchronize_directory(aSource.parent_path())
				&& (aSource.parent_path() == aDestination.parent_path()
					|| synchronize_directory(aDestination.parent_path()));
		}

		[[nodiscard]] auto remove_exact_private_file(
			const std::filesystem::path&   aPath,
			const exact_artifact_identity& aExpected) -> bool
		{
			if (missing(aPath)) {
				return true;
			}
			if (
				aExpected.Kind != exact_artifact_kind::regular_file
				|| exact_identity(aPath, aExpected.Kind) != aExpected
				|| !private_regular_file(aPath)) {
				return false;
			}
			const auto quarantine = random_quarantine_path(aPath, ".bpr-file-cleanup-");
			if (!quarantine.has_value()) {
				return false;
			}
			const auto renamed = rename_exact_to_new_path(aPath, *quarantine, aExpected);
			if (
				!renamed
				&& !(
					missing(aPath)
					&& exact_identity(*quarantine, aExpected.Kind)
						== aExpected
					&& synchronize_directory(aPath.parent_path()))) {
				return false;
			}
			if (exact_identity(*quarantine, aExpected.Kind) != aExpected) {
				return false;
			}
			return remove_file_name(*quarantine)
				&& missing(*quarantine)
				&& synchronize_directory(aPath.parent_path());
		}

		struct installed_pack_artifacts
		{
			exact_artifact_identity Pack;
			exact_artifact_identity Index;

			auto operator<=>(const installed_pack_artifacts&) const = default;
		};

		struct installed_pack_paths
		{
			std::filesystem::path Pack;
			std::filesystem::path Index;
			std::filesystem::path ReverseIndex;
		};

		[[nodiscard]] auto paths_for_installed_pack(
			const std::filesystem::path&     aRepository,
			const application::git_pack_oid& aPack)
			-> std::optional<installed_pack_paths>
		{
			if (!application::valid(aPack)) {
				return std::nullopt;
			}
			const auto digestBytes = aPack.Algorithm == core::git_hash_algorithm::sha1 ? 20U : 32U;
			const auto stem        = "pack-"
				+ lower_hex(std::span{ aPack.Digest }.first(digestBytes));
			const auto directory = aRepository / "objects" / "pack";
			return installed_pack_paths{
				.Pack         = directory / (stem + ".pack"),
				.Index        = directory / (stem + ".idx"),
				.ReverseIndex = directory / (stem + ".rev"),
			};
		}

		enum class recovery_file_kind : std::uint8_t {
			absent,
			private_regular,
			invalid,
		};

		struct recovery_file_observation
		{
			recovery_file_kind                     Kind = recovery_file_kind::absent;
			std::optional<exact_artifact_identity> Identity;
			std::uint64_t                          Bytes = 0U;
		};

		using recovery_ref_pair = std::pair<
			std::optional<application::git_commit_id>,
			std::optional<application::git_commit_id>>;

		[[nodiscard]] constexpr auto classify_recovery_ref_pair(
			const recovery_ref_pair&          aPair,
			const application::git_commit_id& aExpectedBase,
			const application::git_commit_id& aExpectedHead)
			-> application::git_recovery_match
		{
			using application::git_recovery_match;
			if (!aPair.first && !aPair.second) {
				return git_recovery_match::absent;
			}
			if (aPair.first.has_value() != aPair.second.has_value()) {
				return git_recovery_match::ambiguous;
			}
			return aPair.first == aExpectedBase && aPair.second == aExpectedHead
				? git_recovery_match::exact
				: git_recovery_match::mismatch;
		}

		[[nodiscard]] constexpr auto classify_recovery_ref_topology(
			const application::git_recovery_match aStaging,
			const application::git_recovery_match aFinal)
			-> application::git_recovery_ref_topology
		{
			using application::git_recovery_match;
			using application::git_recovery_ref_topology;
			if (
				aStaging == git_recovery_match::absent
				&& aFinal == git_recovery_match::absent) {
				return git_recovery_ref_topology::absent;
			}
			if (
				aStaging == git_recovery_match::exact
				&& aFinal == git_recovery_match::absent) {
				return git_recovery_ref_topology::staging;
			}
			if (
				aStaging == git_recovery_match::absent
				&& aFinal == git_recovery_match::exact) {
				return git_recovery_ref_topology::final;
			}
			return git_recovery_ref_topology::ambiguous;
		}

		[[nodiscard]] auto inspect_recovery_file(
			const std::filesystem::path& aPath) -> recovery_file_observation
		{
			if (missing(aPath)) {
				return { };
			}
			if (!private_regular_file(aPath)) {
				return { .Kind = recovery_file_kind::invalid };
			}
			const auto identity = exact_identity(
				aPath,
				exact_artifact_kind::regular_file);
			std::error_code error;
			const auto      bytes = std::filesystem::file_size(aPath, error);
			return !error && identity.has_value()
				? recovery_file_observation{
					  .Kind     = recovery_file_kind::private_regular,
					  .Identity = identity,
					  .Bytes    = bytes,
				  }
				: recovery_file_observation{ .Kind = recovery_file_kind::invalid };
		}

		[[nodiscard]] auto inspect_installed_pack_artifacts(
			const std::filesystem::path&     aRepository,
			const application::git_pack_oid& aPack)
			-> std::optional<installed_pack_artifacts>
		{
			const auto paths = paths_for_installed_pack(aRepository, aPack);
			if (!paths.has_value()) {
				return std::nullopt;
			}
			const auto packDirectory = paths->Pack.parent_path();
			const auto packIdentity  = exact_identity(
				paths->Pack,
				exact_artifact_kind::regular_file);
			const auto indexIdentity = exact_identity(
				paths->Index,
				exact_artifact_kind::regular_file);
			return private_directory(packDirectory)
					&& private_regular_file(paths->Pack)
					&& private_regular_file(paths->Index)
					&& missing(paths->ReverseIndex) && packIdentity.has_value()
					&& indexIdentity.has_value()
				? std::optional<installed_pack_artifacts>{
					  installed_pack_artifacts{
						  .Pack  = *packIdentity,
						  .Index = *indexIdentity,
					  }
				  }
				: std::nullopt;
		}

		[[nodiscard]] auto rollback_new_pack(
			const std::filesystem::path&     aRepository,
			const application::git_pack_oid& aPack,
			const std::span<const application::git_pack_inventory_entry>
											aPreimportPacks,
			const installed_pack_artifacts& aExpectedArtifacts) -> bool
		{
			const auto digestBytes = aPack.Algorithm == core::git_hash_algorithm::sha1 ? 20U : 32U;
			if (
				!application::valid(aPack)
				|| std::ranges::any_of(
					aPreimportPacks,
					[&](const auto& aEntry) {
						return aEntry.Pack == aPack;
					})) {
				return false;
			}
			const auto packDirectory = aRepository / "objects" / "pack";
			if (!private_directory(packDirectory)) {
				return false;
			}
			const auto stem = "pack-"
				+ lower_hex(
								  std::span{ aPack.Digest }.first(digestBytes));
			const auto packPath    = packDirectory / (stem + ".pack");
			const auto indexPath   = packDirectory / (stem + ".idx");
			const auto reversePath = packDirectory / (stem + ".rev");
			if (
				exact_identity(
					packPath,
					exact_artifact_kind::regular_file)
					!= aExpectedArtifacts.Pack
				|| exact_identity(
					   indexPath,
					   exact_artifact_kind::regular_file)
					!= aExpectedArtifacts.Index
				|| !private_regular_file(packPath)
				|| !private_regular_file(indexPath) || !missing(reversePath)) {
				return false;
			}
			const auto indexQuarantine = random_quarantine_path(indexPath, ".bpr-index-cleanup-");
			const auto packQuarantine  = random_quarantine_path(packPath, ".bpr-pack-cleanup-");
			const auto moveExact =
				[](const std::filesystem::path&   aSource,
				   const std::filesystem::path&   aDestination,
				   const exact_artifact_identity& aIdentity) {
					return rename_exact_to_new_path(
							   aSource,
							   aDestination,
							   aIdentity)
						|| (missing(aSource)
							&& exact_identity(aDestination, aIdentity.Kind)
								== aIdentity
							&& synchronize_directory(
								aDestination.parent_path()));
				};
			if (
				!indexQuarantine.has_value()
				|| !packQuarantine.has_value()
				|| !moveExact(
					indexPath,
					*indexQuarantine,
					aExpectedArtifacts.Index)) {
				return false;
			}
			if (
				!moveExact(
					packPath,
					*packQuarantine,
					aExpectedArtifacts.Pack)) {
				static_cast<void>(moveExact(
					*indexQuarantine,
					indexPath,
					aExpectedArtifacts.Index));
				return false;
			}
			return remove_exact_private_file(
					   *indexQuarantine,
					   aExpectedArtifacts.Index)
				&& remove_exact_private_file(
					   *packQuarantine,
					   aExpectedArtifacts.Pack)
				&& synchronize_directory(packDirectory);
		}

		// Bounds both pack-directory walks of the failed-index cleanup. A larger
		// directory is not cleaned; the inventory refuses it instead.
		inline constexpr std::size_t maximum_pack_directory_walk = 4U * application::maximum_git_cache_pack_count + 16U;

		// `index-pack` names its temporary files from Git's six-character mkstemp
		// template: `tmp_pack_XXXXXX` and `tmp_idx_XXXXXX`.
		[[nodiscard]] auto index_pack_temporary_name(
			const std::filesystem::path::string_type& aName) -> bool
		{
			using character     = std::filesystem::path::value_type;
			const auto prefixed = [&](const std::string_view aPrefix) {
				return aName.size() == aPrefix.size() + 6U
					&& std::equal(aPrefix.begin(), aPrefix.end(), aName.begin(), [](const char aExpected, const character aActual) {
						   return static_cast<character>(aExpected) == aActual;
					   });
			};
			return (prefixed("tmp_pack_") || prefixed("tmp_idx_"))
				&& std::all_of(aName.end() - 6, aName.end(), [](const character aCharacter) {
					   return (aCharacter >= '0' && aCharacter <= '9')
						   || (aCharacter >= 'A' && aCharacter <= 'Z')
						   || (aCharacter >= 'a' && aCharacter <= 'z');
				   });
		}

		/// \brief Lists the pack directory's names before `index-pack` runs.
		/// \details Taken under the operation mutex and the cache lock. No value
		/// means the directory could not be listed within its bound, and the
		/// failed-index cleanup then removes nothing.
		[[nodiscard]] auto pack_directory_names(
			const std::filesystem::path& aRepository)
			-> std::optional<std::vector<std::filesystem::path::string_type>>
		{
			const auto directory = aRepository / "objects" / "pack";
			auto       names     = std::vector<std::filesystem::path::string_type>{ };
			if (missing(directory)) {
				return names;
			}
			if (!private_directory(directory)) {
				return std::nullopt;
			}
			std::error_code error;
			for (auto entry = std::filesystem::directory_iterator{ directory, error };
				 !error && entry != std::filesystem::directory_iterator{ };
				 entry.increment(error)) {
				if (names.size() == maximum_pack_directory_walk) {
					return std::nullopt;
				}
				names.push_back(entry->path().filename().native());
			}
			if (error) {
				return std::nullopt;
			}
			std::ranges::sort(names);
			return names;
		}

		/// \brief Removes the temporary files a failed `index-pack` left behind.
		/// \details An `index-pack` that dies, on a missing object or a truncated
		/// pack, leaves `objects/pack/tmp_pack_XXXXXX` (BPR-0851); a killed one
		/// may too. Only an entry with index-pack's temporary name that was absent
		/// from `aBefore` and is a private, single-link regular file this user owns
		/// is removed, each by exact identity. Everything else stays for the
		/// inventory to refuse, so a store changed any other way still needs repair.
		auto remove_failed_index_residue(
			const std::filesystem::path&                           aRepository,
			const std::vector<std::filesystem::path::string_type>& aBefore) -> void
		{
			const auto directory = aRepository / "objects" / "pack";
			if (!private_directory(directory)) {
				return;
			}
			auto            candidates = std::vector<std::filesystem::path>{ };
			std::size_t     visited    = 0U;
			std::error_code error;
			for (auto entry = std::filesystem::directory_iterator{ directory, error };
				 !error && entry != std::filesystem::directory_iterator{ };
				 entry.increment(error)) {
				if (++visited > maximum_pack_directory_walk) {
					return;
				}
				const auto name = entry->path().filename().native();
				if (index_pack_temporary_name(name) && !std::ranges::binary_search(aBefore, name)) {
					candidates.push_back(entry->path());
				}
			}
			if (error) {
				return;
			}
			auto removed = false;
			for (const auto& candidate : candidates) {
				const auto identity = exact_identity(candidate, exact_artifact_kind::regular_file);
				if (
					identity.has_value() && private_regular_file(candidate)
					&& remove_exact_private_file(candidate, *identity)) {
					removed = true;
				}
			}
			if (removed) {
				static_cast<void>(synchronize_directory(directory));
			}
		}

		[[nodiscard]] auto deletion_marker_present(
			const std::filesystem::path& aStorage,
			const std::string_view       aCacheText)
			-> application::git_cache_result<bool>
		{
			const auto quarantine   = aStorage / "quarantine";
			const auto quarantineV1 = quarantine / "v1";
			if (missing(quarantine)) {
				return false;
			}
			if (!private_directory(quarantine)) {
				return std::unexpected{
					application::git_cache_failure::invalid_path
				};
			}
			if (missing(quarantineV1)) {
				return false;
			}
			if (!private_directory(quarantineV1)) {
				return std::unexpected{
					application::git_cache_failure::invalid_path
				};
			}
			const auto marker = quarantineV1 / (std::string{ aCacheText } + ".deleted");
			if (missing(marker)) {
				return false;
			}
			if (!private_regular_file(marker)) {
				return std::unexpected{
					application::git_cache_failure::repair_required
				};
			}
			return true;
		}

		[[nodiscard]] auto remove_tree_no_follow(
			const std::filesystem::path&   aRoot,
			const exact_artifact_identity& aExpected,
			const std::filesystem::path&   aFaultStorage = { }) -> bool
		{
			if (
				aExpected.Kind != exact_artifact_kind::directory
				|| !private_directory(aRoot)
				|| exact_identity(aRoot, aExpected.Kind) != aExpected) {
				return false;
			}
			struct tree_entry
			{
				std::filesystem::path   Path;
				exact_artifact_identity Identity;
			};
			auto            entries = std::vector<tree_entry>{ };
			std::error_code error;
			auto            entryCount = std::size_t{ 0U };
			auto            iterator   = std::filesystem::recursive_directory_iterator{
				aRoot,
				std::filesystem::directory_options::none,
				error
			};
			const auto end = std::filesystem::recursive_directory_iterator{ };
			while (!error && iterator != end) {
				const auto path   = iterator->path();
				const auto status = std::filesystem::symlink_status(path, error);
				if (error) {
					break;
				}
				if (++entryCount > maximum_repository_entries) {
					return false;
				}
#if defined(Q_OS_WIN)
				const auto attributes = GetFileAttributesW(path.c_str());
				if (attributes == INVALID_FILE_ATTRIBUTES) {
					return false;
				}
				const auto linkLike = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
				const auto linkLike = std::filesystem::is_symlink(status);
#endif
				if (linkLike) {
					return false;
				}
				const auto kind     = status.type()
						== std::filesystem::file_type::directory
					? exact_artifact_kind::directory
					: exact_artifact_kind::regular_file;
				const auto identity = exact_identity(path, kind);
				if (
					!identity.has_value()
					|| identity->Volume != aExpected.Volume) {
					return false;
				}
				entries.push_back({
					.Path     = path,
					.Identity = *identity,
				});
				iterator.increment(error);
			}
			if (
				error
				|| exact_identity(aRoot, aExpected.Kind) != aExpected) {
				return false;
			}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
			std::error_code substitutionInjectionError;
			if (
				!aFaultStorage.empty()
				&& std::filesystem::exists(
					aFaultStorage
						/ "inject-cache-delete-child-substitution",
					substitutionInjectionError)
				&& !substitutionInjectionError) {
				const auto target = std::ranges::find_if(
					entries,
					[&](const auto& aEntry) {
						return aEntry.Path == aRoot / manifest_name;
					});
				if (target == entries.end()) {
					return false;
				}
				std::iter_swap(target, entries.end() - 1);
				const auto                                     displaced = aRoot / ".bpr-test-original-manifest";
				static constexpr std::array<std::uint8_t, 24U> canary{
					'r',
					'e',
					'p',
					'l',
					'a',
					'c',
					'e',
					'm',
					'e',
					'n',
					't',
					'-',
					'c',
					'h',
					'i',
					'l',
					'd',
					'-',
					'c',
					'a',
					'n',
					'a',
					'r',
					'y',
				};
				if (
					!rename_exact_to_new_path(
						(entries.end() - 1)->Path,
						displaced,
						(entries.end() - 1)->Identity)
					|| !write_new_private_file(
						(entries.end() - 1)->Path,
						canary)) {
					return false;
				}
			} else if (substitutionInjectionError) {
				return false;
			}
#else
			static_cast<void>(aFaultStorage);
#endif
			std::ranges::reverse(entries);
			for (const auto& entry : entries) {
				if (
					exact_identity(
						entry.Path,
						entry.Identity.Kind)
					!= entry.Identity) {
					return false;
				}
				std::error_code removeError;
				if (
					!std::filesystem::remove(
						entry.Path,
						removeError)
					|| removeError) {
					return false;
				}
			}
			std::error_code rootError;
			return exact_identity(aRoot, aExpected.Kind) == aExpected
				&& std::filesystem::remove(aRoot, rootError)
				&& !rootError && missing(aRoot);
		}

		[[nodiscard]] auto quarantine_and_remove_exact_tree(
			const std::filesystem::path&   aPath,
			const exact_artifact_identity& aExpected) -> bool
		{
			const auto quarantine = random_quarantine_path(aPath, ".bpr-tree-cleanup-");
			if (!quarantine.has_value()) {
				return false;
			}
			const auto renamed = rename_exact_to_new_path(aPath, *quarantine, aExpected);
			if (
				!renamed
				&& !(
					missing(aPath)
					&& exact_identity(*quarantine, aExpected.Kind)
						== aExpected
					&& synchronize_directory(aPath.parent_path()))) {
				return false;
			}
			return remove_tree_no_follow(*quarantine, aExpected)
				&& synchronize_directory(aPath.parent_path());
		}

		class installed_git_cache_session final
			: public application::git_cache_session
		{
			class blob_collector final
				: public application::git_blob_materialization_sink
			{
			public:
				auto accept(
					const application::git_blob_id&  aBlob,
					const std::span<const std::byte> aBytes) -> void override
				{
					Blob = aBlob;
					Bytes.resize(aBytes.size());
					if (!aBytes.empty()) {
						std::memcpy(Bytes.data(), aBytes.data(), aBytes.size());
					}
				}

				std::optional<application::git_blob_id> Blob;
				std::vector<std::uint8_t>               Bytes;
			};

		public:
			explicit installed_git_cache_session(
				std::shared_ptr<cache_state> aState)
				: State{ std::move(aState) }
			{
			}

			[[nodiscard]] auto request() const noexcept
				-> const application::git_cache_open_request& override
			{
				return State->Request;
			}

			[[nodiscard]] auto create_pack_sink(
				const core::epoch_operation_id                                      aOperation,
				const std::uint64_t                                                 aMaximumBytes,
				const std::stop_token                                               aStop,
				const std::function<bool(const application::git_spool_ownership&)>& aWitness)
				-> application::git_cache_result<
					std::unique_ptr<application::git_pack_stream_sink>> override
			{
				// On initialization failure, release the lock before the sink destructor
				// acquires it for exact cleanup. Declaration order also covers exceptions.
				std::unique_ptr<pack_sink> sink;
				std::lock_guard            operationGuard{ State->OperationMutex };
				if (
					aStop.stop_requested() || !aOperation.valid()
					|| aMaximumBytes == 0U
					|| aMaximumBytes > State->Request.Limits.OperationPackBytes) {
					return std::unexpected{
						aStop.stop_requested()
							? application::git_cache_failure::cancelled
							: application::git_cache_failure::
								  invalid_request
					};
				}
				const auto inventory = State->validated_inventory(aStop);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto completed = State->Process->pack_spool_path(
					process_operation(aOperation));
				if (!completed.has_value()) {
					return std::unexpected{
						State->process_failure(completed.error())
					};
				}
				const auto receiving = completed->parent_path()
					/ ("." + lower_hex(aOperation.bytes())
					   + ".pack.receiving");
				if (!missing(*completed) || !missing(receiving)) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				if (const auto admitted = State->admit_incoming_file(aStop); !admitted) {
					return std::unexpected{ admitted.error() };
				}
				sink = std::make_unique<pack_sink>(
					State,
					aOperation,
					receiving,
					*completed,
					aMaximumBytes,
					State->Request.ObjectFormat);
				if (!sink->initialize(aWitness)) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				return std::unique_ptr<application::git_pack_stream_sink>{
					std::move(sink)
				};
			}

			[[nodiscard]] auto observe_preimport(
				const application::completed_git_pack_receipt& aReceipt,
				const std::stop_token                          aStop)
				-> application::git_cache_result<
					application::git_preimport_observation> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (
					!application::agrees(aReceipt)
					|| State->operation_aborted(aReceipt.Operation)
					|| aReceipt.ExpectedPack.Algorithm
						!= State->Request.ObjectFormat
					|| aReceipt.Spool.Bytes
						> State->Request.Limits.OperationPackBytes) {
					return std::unexpected{
						application::git_cache_failure::
							inventory_mismatch
					};
				}
				const auto recorded = State->find_receipt(aReceipt);
				if (
					!recorded.has_value()
					|| !private_regular_file(recorded->Spool)
					|| exact_identity(
						   recorded->Spool,
						   exact_artifact_kind::regular_file)
						!= recorded->SpoolIdentity) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				const auto inventory = State->validated_inventory(aStop);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto preexisting = std::ranges::any_of(
					inventory->Packs,
					[&](const auto& aEntry) {
						return aEntry.Pack == aReceipt.ExpectedPack;
					});
				const auto refBytes       = ref_pair_bytes(State->Request.ObjectFormat);
				auto       admissionBytes = refBytes;
				if (!preexisting) {
					if (
						aReceipt.Spool.Bytes
							> std::numeric_limits<std::uint64_t>::max()
								- recorded->MaximumIndexBytes
						|| aReceipt.Spool.Bytes
								+ recorded->MaximumIndexBytes
							> std::numeric_limits<std::uint64_t>::max()
								- admissionBytes) {
						return std::unexpected{
							application::git_cache_failure::quota_exceeded
						};
					}
					admissionBytes += aReceipt.Spool.Bytes
						+ recorded->MaximumIndexBytes;
				}
				if (
					!fits_within(
						inventory->RepositoryBytes,
						admissionBytes,
						State->Request.Limits.RepositoryBytes)
					|| (inventory->Packs.size()
							>= State->Request.Limits.PackCount
						&& !preexisting)) {
					return std::unexpected{
						application::git_cache_failure::quota_exceeded
					};
				}
				auto observation = application::git_preimport_observation{
					.Operation               = aReceipt.Operation,
					.ExpectedPack            = aReceipt.ExpectedPack,
					.Packs                   = inventory->Packs,
					.RepositoryBytes         = inventory->RepositoryBytes,
					.ExpectedPackPreexisting = preexisting,
					.OpaqueToken             = State->new_token(),
				};
				if (
					!State->register_observation({
						.Observation       = observation,
						.Receipt           = aReceipt,
						.MaximumIndexBytes = recorded->MaximumIndexBytes,
					})) {
					return std::unexpected{
						application::git_cache_failure::
							inventory_mismatch
					};
				}
				return observation;
			}

			[[nodiscard]] auto prepare_revision(
				const application::git_prepare_revision_request&                      aRequest,
				const std::stop_token                                                 aStop,
				const std::function<bool(const application::git_prepared_revision&)>& aWitness)
				-> application::git_cache_result<
					application::git_prepared_revision> override
			{
				return prepare_revision_impl(aRequest, aStop, aWitness, false);
			}

			[[nodiscard]] auto prepare_sparse_revision(
				const application::git_prepare_revision_request& aRequest, std::stop_token aStop,
				const std::function<bool(const application::git_prepared_revision&)>& aWitness)
				-> application::git_cache_result<application::git_prepared_revision> override
			{
				if (!aWitness || aRequest.DeriveComparisonBase || aRequest.Base.algorithm() != core::git_hash_algorithm::sha1) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				return prepare_revision_impl(aRequest, aStop, aWitness, true);
			}

			[[nodiscard]] auto prepare_revision_impl(
				const application::git_prepare_revision_request& aRequest, std::stop_token aStop,
				const std::function<bool(const application::git_prepared_revision&)>& aWitness, bool aSparse)
				-> application::git_cache_result<application::git_prepared_revision>
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				const auto recordedObservation = State->find_observation(
					aRequest.Inventory,
					aRequest.Receipt);
				if (
					aRequest.Base.algorithm() != State->Request.ObjectFormat
					|| State->operation_aborted(
						aRequest.Receipt.Operation)
					|| aRequest.Authorization.ExpectedPack.Algorithm
						!= State->Request.ObjectFormat
					|| !recordedObservation.has_value()) {
					return std::unexpected{
						application::git_cache_failure::
							authorization_mismatch
					};
				}
				const auto current = State->validated_inventory(aStop);
				if (
					!current.has_value()
					|| current->Packs != aRequest.Inventory.Packs
					|| current->RepositoryBytes
						!= aRequest.Inventory.RepositoryBytes) {
					return std::unexpected{
						current.has_value()
							? application::git_cache_failure::
								  inventory_mismatch
							: current.error()
					};
				}
				const auto refBytes       = ref_pair_bytes(State->Request.ObjectFormat);
				auto       admissionBytes = refBytes;
				if (!aRequest.Inventory.ExpectedPackPreexisting) {
					if (
						aRequest.Receipt.Spool.Bytes
							> std::numeric_limits<std::uint64_t>::max()
								- recordedObservation->MaximumIndexBytes
						|| aRequest.Receipt.Spool.Bytes
								+ recordedObservation->MaximumIndexBytes
							> std::numeric_limits<std::uint64_t>::max()
								- admissionBytes) {
						return std::unexpected{
							application::git_cache_failure::quota_exceeded
						};
					}
					admissionBytes += aRequest.Receipt.Spool.Bytes
						+ recordedObservation->MaximumIndexBytes;
				}
				if (
					current->PrivateRefs
						> State->Request.Limits.PrivateRefCount - 2U
					|| !fits_within(
						current->RepositoryBytes,
						admissionBytes,
						State->Request.Limits.RepositoryBytes)
					|| (current->Packs.size()
							>= State->Request.Limits.PackCount
						&& !aRequest.Inventory.ExpectedPackPreexisting)) {
					return std::unexpected{
						application::git_cache_failure::quota_exceeded
					};
				}
				const auto processExpected = process_pack(aRequest.Receipt.ExpectedPack);
				auto       base            = process_commit(aRequest.Base);
				const auto head            = process_commit(aRequest.Head);
				if (
					!processExpected.has_value() || !base.has_value()
					|| !head.has_value()) {
					return std::unexpected{
						application::git_cache_failure::
							object_mismatch
					};
				}
				const auto recordedReceipt = State->find_receipt(aRequest.Receipt);
				if (
					!recordedReceipt.has_value()
					|| !private_regular_file(recordedReceipt->Spool)
					|| exact_identity(
						   recordedReceipt->Spool,
						   exact_artifact_kind::regular_file)
						!= recordedReceipt->SpoolIdentity) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				// Plants a file with index-pack's temporary name before the snapshot,
				// so the failed-index cleanup must leave it (BPR-0851).
				std::error_code preexistingResidueInjectionError;
				if (
					std::filesystem::exists(State->StorageRoot / "inject-cache-preexisting-index-residue", preexistingResidueInjectionError)
					&& !preexistingResidueInjectionError) {
					static constexpr std::array<std::uint8_t, 7U> planted{ 'p', 'l', 'a', 'n', 't', 'e', 'd' };
					static_cast<void>(write_new_private_file(State->Repository / "objects" / "pack" / "tmp_pack_BPRpre", planted));
				}
#endif
				// The failed-index cleanup removes only what this child could have
				// created, so the snapshot is taken last, under the lock (BPR-0851).
				const auto packNamesBeforeIndex = pack_directory_names(State->Repository);
				const auto indexPack            = aSparse ? &application::git_process_port::index_sparse_pack : &application::git_process_port::index_pack;
				auto       indexed              = (State->Process.get()->*indexPack)(
					application::git_index_pack_request{
						.Repository        = State->Repository,
						.Operation         = process_operation(aRequest.Receipt.Operation),
						.SpoolBytes        = aRequest.Receipt.Spool.Bytes,
						.MaximumInputBytes = State->Request.Limits.OperationPackBytes,
						.ExpectedPack      = *processExpected,
					},
					aStop);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				std::error_code lockSubstitutionInjectionError;
				if (
					std::filesystem::exists(
						State->StorageRoot
							/ "inject-cache-lock-substitution-during-index",
						lockSubstitutionInjectionError)
					&& !lockSubstitutionInjectionError) {
					const auto displacedLock = State->LockPath.parent_path()
						/ (".bpr-test-original-"
						   + State->LockPath.filename().string());
					static constexpr std::array<std::uint8_t, 23U>
						canary{
							'r',
							'e',
							'p',
							'l',
							'a',
							'c',
							'e',
							'm',
							'e',
							'n',
							't',
							'-',
							'l',
							'o',
							'c',
							'k',
							'-',
							'c',
							'a',
							'n',
							'a',
							'r',
							'y',
						};
					if (
						!rename_exact_to_new_path(
							State->LockPath,
							displacedLock,
							State->LockIdentity)
						|| !write_new_private_file(
							State->LockPath,
							canary)) {
						if (
							!indexed.has_value()
							&& indexed.error()
								== application::git_process_failure::
									reap_failure) {
							static_cast<void>(
								State->process_failure(indexed.error()));
						}
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
				} else if (lockSubstitutionInjectionError) {
					if (
						!indexed.has_value()
						&& indexed.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(indexed.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
#endif
				if (!State->active_boundary_still_owned()) {
					if (
						!indexed.has_value()
						&& indexed.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(indexed.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				const auto importedArtifacts = aRequest.Inventory.ExpectedPackPreexisting
					? std::optional<installed_pack_artifacts>{ }
					: inspect_installed_pack_artifacts(
						  State->Repository,
						  aRequest.Receipt.ExpectedPack);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				std::error_code injectionError;
				if (
					indexed.has_value()
					&& std::filesystem::exists(
						State->StorageRoot
							/ "inject-cache-index-terminal-failure",
						injectionError)
					&& !injectionError) {
					indexed = std::unexpected{
						application::git_process_failure::malformed_output
					};
				}
#endif
				if (!indexed.has_value()) {
					const auto failure = State->process_failure(indexed.error());
					if (
						indexed.error()
						== application::git_process_failure::reap_failure) {
						return std::unexpected{ failure };
					}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
					// Plants, after the snapshot, entries the failed-index cleanup must
					// leave: an extra pack, or temporary names that are hard-linked or,
					// on POSIX, not private (BPR-0851).
					std::error_code                               residueInjectionError;
					const auto                                    packDirectory = State->Repository / "objects" / "pack";
					static constexpr std::array<std::uint8_t, 7U> planted{ 'p', 'l', 'a', 'n', 't', 'e', 'd' };
					if (
						std::filesystem::exists(State->StorageRoot / "inject-cache-extra-pack-during-index", residueInjectionError)
						&& !residueInjectionError) {
						const auto stem = std::string{ "pack-" }
							+ std::string(State->Request.ObjectFormat == core::git_hash_algorithm::sha1 ? 40U : 64U, 'e');
						static_cast<void>(write_new_private_file(packDirectory / (stem + ".pack"), planted));
						static_cast<void>(write_new_private_file(packDirectory / (stem + ".idx"), planted));
					}
					if (
						std::filesystem::exists(State->StorageRoot / "inject-cache-foreign-index-residue-during-index", residueInjectionError)
						&& !residueInjectionError) {
						if (write_new_private_file(packDirectory / "tmp_pack_BPRln1", planted)) {
							std::filesystem::create_hard_link(packDirectory / "tmp_pack_BPRln1", packDirectory / "tmp_idx_BPRln2", residueInjectionError);
						}
	#if defined(Q_OS_UNIX)
						if (write_new_private_file(packDirectory / "tmp_idx_BPRshr", planted)) {
							std::filesystem::permissions(packDirectory / "tmp_idx_BPRshr", std::filesystem::perms::group_read, std::filesystem::perm_options::add, residueInjectionError);
						}
	#endif
					}
#endif
					if (packNamesBeforeIndex.has_value()) {
						remove_failed_index_residue(State->Repository, *packNamesBeforeIndex);
					}
					const auto afterFailure = State->validated_inventory({ }, false);
					if (!afterFailure.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					if (same_inventory(*current, *afterFailure)) {
						return std::unexpected{ failure };
					}
					const auto priorPacksUnchanged = std::ranges::all_of(
						current->Packs,
						[&](const auto& aPrior) {
							return std::ranges::find(
									   afterFailure->Packs,
									   aPrior)
								!= afterFailure->Packs.end();
						});
					const auto expectedPresent = std::ranges::any_of(
						afterFailure->Packs,
						[&](const auto& aEntry) {
							return aEntry.Pack
								== aRequest.Receipt.ExpectedPack;
						});
					const auto onlyExpectedPackAdded = !aRequest.Inventory.ExpectedPackPreexisting
						&& importedArtifacts.has_value()
						&& expectedPresent && priorPacksUnchanged
						&& afterFailure->Packs.size()
							== current->Packs.size() + 1U
						&& afterFailure->PrivateRefs
							== current->PrivateRefs;
					if (
						!onlyExpectedPackAdded
						|| !rollback_new_pack(
							State->Repository,
							aRequest.Receipt.ExpectedPack,
							current->Packs,
							*importedArtifacts)) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					const auto reconciled = State->validated_inventory({ }, false);
					return reconciled.has_value()
							&& same_inventory(*current, *reconciled)
						? application::git_cache_result<
							  application::git_prepared_revision>{
							  std::unexpected{ failure }
						  }
						: application::git_cache_result<application::git_prepared_revision>{ std::unexpected{ application::git_cache_failure::repair_required } };
				}
				if (
					!aRequest.Inventory.ExpectedPackPreexisting
					&& !importedArtifacts.has_value()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				const auto rollbackFailure =
					[&](application::git_cache_error aFailure)
					-> application::git_cache_result<
						application::git_prepared_revision> {
					if (aRequest.Inventory.ExpectedPackPreexisting) {
						return std::unexpected{ std::move(aFailure) };
					}
					const auto rolledBack = rollback_new_pack(
						State->Repository,
						aRequest.Receipt.ExpectedPack,
						current->Packs,
						*importedArtifacts);
					const auto reconciled = rolledBack
						? State->validated_inventory({ }, false)
						: application::git_cache_result<
							  inventory_snapshot>{
							  std::unexpected{
								  application::git_cache_failure::
									  repair_required }
						  };
					return reconciled.has_value()
							&& same_inventory(*current, *reconciled)
						? application::git_cache_result<
							  application::git_prepared_revision>{
							  std::unexpected{ std::move(aFailure) }
						  }
						: application::git_cache_result<application::git_prepared_revision>{ std::unexpected{ application::git_cache_failure::repair_required } };
				};
				if (indexed->Pack != *processExpected) {
					return rollbackFailure(
						application::git_cache_failure::object_mismatch);
				}

				const auto afterImport = State->validated_inventory({ }, false);
				if (!afterImport.has_value()) {
					return rollbackFailure(afterImport.error());
				}
				const auto expectedPack = std::ranges::find_if(
					afterImport->Packs,
					[&](const auto& aEntry) {
						return aEntry.Pack == aRequest.Receipt.ExpectedPack;
					});
				const auto priorPacksUnchanged = std::ranges::all_of(
					current->Packs,
					[&](const auto& aPrior) {
						return std::ranges::find(
								   afterImport->Packs,
								   aPrior)
							!= afterImport->Packs.end();
					});
				const auto expectedPackCount = current->Packs.size()
					+ (aRequest.Inventory.ExpectedPackPreexisting ? 0U : 1U);
				if (
					expectedPack == afterImport->Packs.end()
					|| !priorPacksUnchanged
					|| afterImport->Packs.size() != expectedPackCount
					|| afterImport->Packs.size()
						> State->Request.Limits.PackCount
					|| !fits_within(
						afterImport->RepositoryBytes,
						refBytes,
						State->Request.Limits.RepositoryBytes)
					|| afterImport->PrivateRefs
						> State->Request.Limits.PrivateRefCount - 2U) {
					return rollbackFailure(
						application::git_cache_failure::quota_exceeded);
				}
				const auto preparedArtifacts = inspect_installed_pack_artifacts(
					State->Repository,
					aRequest.Receipt.ExpectedPack);
				if (
					!preparedArtifacts.has_value()
					|| (importedArtifacts.has_value()
						&& *preparedArtifacts
							!= *importedArtifacts)) {
					return rollbackFailure(
						application::git_cache_failure::
							repair_required);
				}
				const auto durablePackIdentity  = artifact_identity(preparedArtifacts->Pack);
				const auto durableIndexIdentity = artifact_identity(preparedArtifacts->Index);
				if (
					!durablePackIdentity.has_value()
					|| !durableIndexIdentity.has_value()) {
					return rollbackFailure(
						application::git_cache_failure::
							repair_required);
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (aRequest.DeriveComparisonBase) {
					const auto derived = State->Process->derive_comparison_base(
						{ .Repository = State->Repository, .Head = *head, .SelectedBase = *base }, aStop);
					if (!State->active_boundary_still_owned()) {
						if (!derived && derived.error() == application::git_process_failure::reap_failure) {
							static_cast<void>(State->process_failure(derived.error()));
						}
						return std::unexpected{ application::git_cache_failure::repair_required };
					}
					if (!derived) {
						if (derived.error() == application::git_process_failure::reap_failure) {
							return std::unexpected{ State->process_failure(derived.error()) };
						}
						return rollbackFailure(State->process_failure(derived.error()));
					}
					if (derived->Status != application::git_comparison_base_status::unique || !derived->Base) {
						return rollbackFailure(application::git_cache_failure::object_mismatch);
					}
					base = derived->Base;
				}
				const auto baseInspection = State->Process->inspect_commit(
					application::git_commit_request{
						.Repository = State->Repository,
						.Commit     = *base,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!baseInspection.has_value()
						&& baseInspection.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(
								baseInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!baseInspection.has_value()) {
					if (
						baseInspection.error()
						!= application::git_process_failure::reap_failure) {
						return rollbackFailure(
							State->process_failure(
								baseInspection.error()));
					}
					return std::unexpected{
						State->process_failure(
							baseInspection.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				const auto headInspection = State->Process->inspect_commit(
					application::git_commit_request{
						.Repository = State->Repository,
						.Commit     = *head,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!headInspection.has_value()
						&& headInspection.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(
								headInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!headInspection.has_value()) {
					if (
						headInspection.error()
						!= application::git_process_failure::reap_failure) {
						return rollbackFailure(
							State->process_failure(
								headInspection.error()));
					}
					return std::unexpected{
						State->process_failure(
							headInspection.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				auto connected = application::git_process_result<void>{ };
				if (!aSparse) {
					connected = State->Process->verify_connectivity(
						{ .Repository = State->Repository, .Base = *base, .Head = *head }, aStop);
				}
				if (!State->active_boundary_still_owned()) {
					if (
						!connected.has_value()
						&& connected.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(connected.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!connected.has_value()) {
					if (
						connected.error()
						!= application::git_process_failure::reap_failure) {
						return rollbackFailure(
							State->process_failure(connected.error()));
					}
					return std::unexpected{
						State->process_failure(connected.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				auto headLineage = aSparse && aRequest.PriorHead ? core::head_lineage::unknown : core::head_lineage::not_applicable;
				if (
					!aSparse && aRequest.PriorHead.has_value()
					&& *aRequest.PriorHead != aRequest.Head) {
					const auto priorHead = process_commit(*aRequest.PriorHead);
					if (!priorHead.has_value()) {
						return rollbackFailure(
							application::git_cache_failure::object_mismatch);
					}
					const auto ancestry = State->Process->is_ancestor(
						application::git_ancestry_request{
							.Repository = State->Repository,
							.Ancestor   = *priorHead,
							.Descendant = *head,
						},
						aStop);
					if (!State->active_boundary_still_owned()) {
						if (
							!ancestry.has_value()
							&& ancestry.error()
								== application::git_process_failure::
									reap_failure) {
							static_cast<void>(
								State->process_failure(ancestry.error()));
						}
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					if (!ancestry.has_value()) {
						if (
							ancestry.error()
							== application::git_process_failure::
								object_unavailable) {
							headLineage = core::head_lineage::unknown;
						} else if (
							ancestry.error()
							!= application::git_process_failure::
								reap_failure) {
							return rollbackFailure(
								State->process_failure(ancestry.error()));
						} else {
							return std::unexpected{
								State->process_failure(ancestry.error())
							};
						}
					} else {
						headLineage = *ancestry == application::git_ancestry::ancestor
							? core::head_lineage::descendant
							: core::head_lineage::non_fast_forward;
					}
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				const auto prepared = application::git_prepared_revision{
					.Operation     = aRequest.Receipt.Operation,
					.Pack          = aRequest.Receipt.ExpectedPack,
					.InstalledPack = {
						.Role = application::git_artifact_role::
							installed_pack,
						.Identity = *durablePackIdentity,
						.Bytes    = expectedPack->PackBytes,
					},
					.InstalledIndex = {
						.Role     = application::git_artifact_role::installed_index,
						.Identity = *durableIndexIdentity,
						.Bytes    = expectedPack->IndexBytes,
					},
					.Base          = core_commit(*base),
					.BaseTree      = core_tree(baseInspection->Tree),
					.Head          = aRequest.Head,
					.HeadTree      = core_tree(headInspection->Tree),
					.HeadLineage   = headLineage,
					.ImportedBytes = aRequest.Receipt.Spool.Bytes,
					.Residency     = aSparse ? application::git_revision_residency::sparse_commit_trees : application::git_revision_residency::complete_closure,
				};
				if (aWitness && !aWitness(prepared)) {
					// The catalog may have committed despite an unavailable reply.
					// Leave the installed objects and spool for exact recovery.
					return std::unexpected{ application::git_cache_failure::authorization_mismatch };
				}
				if (!State->active_boundary_still_owned()
					|| inspect_installed_pack_artifacts(State->Repository, aRequest.Receipt.ExpectedPack) != preparedArtifacts) {
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				const std::array<application::git_ref_mutation, 2U>
						   staging{
							   application::git_ref_write{
								   .Ref = staging_ref(
									   aRequest.Receipt.Operation,
									   application::git_private_ref_endpoint::base),
								   .DesiredObject = *base,
								   .ExpectedOld   = application::git_ref_expected_absent{ },
							   },
							   application::git_ref_write{
								   .Ref = staging_ref(
									   aRequest.Receipt.Operation,
									   application::git_private_ref_endpoint::head),
								   .DesiredObject = *head,
								   .ExpectedOld   = application::git_ref_expected_absent{ },
							   },
						   };
				const auto staged = State->Process->update_refs(
					application::git_update_refs_request{
						.Repository = State->Repository,
						.Mutations  = staging,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!staged.has_value()
						&& staged.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(staged.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!staged.has_value()) {
					// A cancelled or failed update-ref child may have crossed its
					// commit point before its terminal output was observed. Retain
					// the admitted pack and completed spool for exact recovery;
					// deleting either here could strand staging refs.
					return std::unexpected{
						State->process_failure(staged.error())
					};
				}
				if (
					!remove_exact_private_file(
						recordedReceipt->Spool,
						recordedReceipt->SpoolIdentity)) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				if (
					!State->active_boundary_still_owned()
					|| inspect_installed_pack_artifacts(
						   State->Repository,
						   aRequest.Receipt.ExpectedPack)
						!= preparedArtifacts) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				return prepared;
			}

			[[nodiscard]] auto promote_retained(
				const application::git_prepared_revision& aPrepared,
				const application::git_ref_promotion_authorization&
									  aAuthorization,
				const std::stop_token aStop)
				-> application::git_cache_result<
					application::git_retained_ref_pair> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (
					!application::valid(aPrepared)
					|| !application::valid(aAuthorization)
					|| aPrepared.Operation != aAuthorization.Operation
					|| State->operation_aborted(aPrepared.Operation)
					|| aPrepared.Base != aAuthorization.Base
					|| aPrepared.Head != aAuthorization.Head
					|| aAuthorization.Scope
						!= State->Request.Authority.Scope
					|| aAuthorization.Base.algorithm()
						!= State->Request.ObjectFormat) {
					return std::unexpected{
						application::git_cache_failure::
							authorization_mismatch
					};
				}
				const auto inventory = State->validated_inventory(aStop);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto installed = inspect_installed_pack_artifacts(
					State->Repository,
					aPrepared.Pack);
				const auto installedInventory = std::ranges::find_if(
					inventory->Packs,
					[&](const auto& aEntry) {
						return aEntry.Pack == aPrepared.Pack;
					});
				if (
					!installed.has_value()
					|| installedInventory
						== inventory->Packs.end()
					|| exact_identity(
						   aPrepared.InstalledPack.Identity)
						!= installed->Pack
					|| exact_identity(
						   aPrepared.InstalledIndex.Identity)
						!= installed->Index
					|| aPrepared.InstalledPack.Bytes
						!= installedInventory->PackBytes
					|| aPrepared.InstalledIndex.Bytes
						!= installedInventory->IndexBytes) {
					return std::unexpected{
						application::git_cache_failure::
							authorization_mismatch
					};
				}
				const auto base = process_commit(aPrepared.Base);
				const auto head = process_commit(aPrepared.Head);
				if (!base.has_value() || !head.has_value()) {
					return std::unexpected{
						application::git_cache_failure::
							object_mismatch
					};
				}
				const auto inspected = inspect_pair(
					staging_ref(
						aPrepared.Operation,
						application::git_private_ref_endpoint::base),
					staging_ref(
						aPrepared.Operation,
						application::git_private_ref_endpoint::head),
					aStop);
				if (
					!inspected.has_value() || inspected->first != *base
					|| inspected->second != *head) {
					return std::unexpected{
						inspected.has_value()
							? application::git_cache_failure::ref_conflict
							: inspected.error()
					};
				}
				const auto finalBase = retained_ref(
					aAuthorization.Scope,
					aAuthorization.Ordinal,
					application::git_private_ref_endpoint::base);
				const auto finalHead = retained_ref(
					aAuthorization.Scope,
					aAuthorization.Ordinal,
					application::git_private_ref_endpoint::head);
				const auto absent = inspect_pair_optional(
					finalBase,
					finalHead,
					aStop);
				if (
					!absent.has_value() || absent->first.has_value()
					|| absent->second.has_value()) {
					return std::unexpected{
						absent.has_value()
							? application::git_cache_failure::ref_conflict
							: absent.error()
					};
				}
				const std::array<application::git_ref_mutation, 4U>
					mutations{
						application::git_ref_write{
							.Ref           = finalBase,
							.DesiredObject = *base,
							.ExpectedOld   = application::git_ref_expected_absent{ },
						},
						application::git_ref_write{
							.Ref           = finalHead,
							.DesiredObject = *head,
							.ExpectedOld   = application::git_ref_expected_absent{ },
						},
						application::git_ref_remove{
							.Ref = staging_ref(
								aPrepared.Operation,
								application::git_private_ref_endpoint::base),
							.ExpectedOld = *base,
						},
						application::git_ref_remove{
							.Ref = staging_ref(
								aPrepared.Operation,
								application::git_private_ref_endpoint::head),
							.ExpectedOld = *head,
						},
					};
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto promoted = State->Process->update_refs(
					application::git_update_refs_request{
						.Repository = State->Repository,
						.Mutations  = mutations,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!promoted.has_value()
						&& promoted.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(promoted.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!promoted.has_value()) {
					return std::unexpected{
						State->process_failure(promoted.error())
					};
				}
				State->forget_operation(aPrepared.Operation);
				return application::git_retained_ref_pair{
					.Scope     = aAuthorization.Scope,
					.Ordinal   = aAuthorization.Ordinal,
					.Base      = aPrepared.Base,
					.Head      = aPrepared.Head,
					.Residency = aPrepared.Residency,
				};
			}

			[[nodiscard]] auto inspect_retained(
				const core::epoch_ordinal aOrdinal,
				const std::stop_token     aStop)
				-> application::git_cache_result<
					application::git_retained_ref_pair> override
			{
				return inspect_retained_impl(aOrdinal, aStop, false);
			}

			[[nodiscard]] auto inspect_sparse_retained(core::epoch_ordinal aOrdinal, std::stop_token aStop)
				-> application::git_cache_result<application::git_retained_ref_pair> override
			{
				if (State->Request.ObjectFormat != core::git_hash_algorithm::sha1) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				return inspect_retained_impl(aOrdinal, aStop, true);
			}

			[[nodiscard]] auto inspect_retained_impl(core::epoch_ordinal aOrdinal, std::stop_token aStop, bool aSparse)
				-> application::git_cache_result<application::git_retained_ref_pair>
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!core::valid(aOrdinal)) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto pair = inspect_pair(
					retained_ref(
						State->Request.Authority.Scope,
						aOrdinal,
						application::git_private_ref_endpoint::base),
					retained_ref(
						State->Request.Authority.Scope,
						aOrdinal,
						application::git_private_ref_endpoint::head),
					aStop);
				if (!pair.has_value()) {
					return std::unexpected{ pair.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto baseInspection = State->Process->inspect_commit(
					application::git_commit_request{
						.Repository = State->Repository,
						.Commit     = pair->first,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!baseInspection.has_value()
						&& baseInspection.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(
								baseInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!baseInspection.has_value()) {
					return std::unexpected{
						State->process_failure(
							baseInspection.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto headInspection = State->Process->inspect_commit(
					application::git_commit_request{
						.Repository = State->Repository,
						.Commit     = pair->second,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!headInspection.has_value()
						&& headInspection.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(
								headInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!headInspection.has_value()) {
					return std::unexpected{
						State->process_failure(
							headInspection.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				auto connected = application::git_process_result<void>{ };
				if (!aSparse) {
					connected = State->Process->verify_connectivity(
						{ .Repository = State->Repository, .Base = pair->first, .Head = pair->second }, aStop);
				}
				if (!State->active_boundary_still_owned()) {
					if (
						!connected.has_value()
						&& connected.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(connected.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!connected.has_value()) {
					return std::unexpected{
						State->process_failure(connected.error())
					};
				}
				return application::git_retained_ref_pair{
					.Scope     = State->Request.Authority.Scope,
					.Ordinal   = aOrdinal,
					.Base      = core_commit(pair->first),
					.Head      = core_commit(pair->second),
					.Residency = aSparse ? application::git_revision_residency::sparse_commit_trees : application::git_revision_residency::complete_closure,
				};
			}

			[[nodiscard]] auto inspect_commit_object(
				const core::git_commit_oid aCommit,
				const std::stop_token      aStop)
				-> application::git_cache_result<
					application::git_cache_commit_inspection> override
			{
				return inspect_commit_object_impl(aCommit, aStop, false);
			}

			[[nodiscard]] auto inspect_sparse_commit_object(core::git_commit_oid aCommit, std::stop_token aStop)
				-> application::git_cache_result<application::git_cache_commit_inspection> override
			{
				if (State->Request.ObjectFormat != core::git_hash_algorithm::sha1) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				return inspect_commit_object_impl(aCommit, aStop, true);
			}

			[[nodiscard]] auto inspect_commit_object_impl(core::git_commit_oid aCommit, std::stop_token aStop, bool aSparse)
				-> application::git_cache_result<application::git_cache_commit_inspection>
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				const auto expected = process_commit(aCommit);
				if (
					!expected.has_value()
					|| aCommit.algorithm() != State->Request.ObjectFormat) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::invalid_repository
					};
				}
				const auto inspected = State->Process->inspect_commit(
					application::git_commit_request{
						.Repository = State->Repository,
						.Commit     = *expected,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!inspected.has_value()
						&& inspected.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(inspected.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!inspected.has_value()) {
					return std::unexpected{
						inspected.error()
									== application::git_process_failure::object_unavailable
								|| inspected.error()
									== application::git_process_failure::nonzero_exit
							? application::git_cache_failure::object_mismatch
							: State->process_failure(inspected.error())
					};
				}
				auto connected = application::git_process_result<void>{ };
				if (!aSparse) {
					connected = State->Process->verify_connectivity(
						{ .Repository = State->Repository, .Base = *expected, .Head = *expected }, aStop);
				}
				if (!State->active_boundary_still_owned()) {
					if (
						!connected.has_value()
						&& connected.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(connected.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!connected.has_value()) {
					return std::unexpected{
						connected.error()
									== application::git_process_failure::object_unavailable
								|| connected.error()
									== application::git_process_failure::nonzero_exit
							? application::git_cache_failure::object_mismatch
							: State->process_failure(connected.error())
					};
				}
				return application::git_cache_commit_inspection{
					.Commit    = aCommit,
					.Tree      = core_tree(inspected->Tree),
					.Residency = aSparse ? application::git_revision_residency::sparse_commit_trees : application::git_revision_residency::complete_closure,
				};
			}

			[[nodiscard]] auto derive_comparison_base(
				const application::git_cache_comparison_base_request& aRequest,
				const std::stop_token                                 aStop)
				-> application::git_cache_result<application::git_cache_comparison_base_observation> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				const auto head = process_commit(aRequest.Head);
				const auto base = process_commit(aRequest.SelectedBase);
				if (!head || !base || aRequest.Head.algorithm() != State->Request.ObjectFormat
					|| aRequest.SelectedBase.algorithm() != State->Request.ObjectFormat) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				const auto found = State->Process->derive_comparison_base(
					{ .Repository = State->Repository, .Head = *head, .SelectedBase = *base }, aStop);
				if (!State->active_boundary_still_owned()) {
					if (!found && found.error() == application::git_process_failure::reap_failure) {
						static_cast<void>(State->process_failure(found.error()));
					}
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				if (!found) {
					return std::unexpected{ State->process_failure(found.error()) };
				}
				return application::git_cache_comparison_base_observation{
					.Status = found->Status,
					.Base   = found->Base ? std::optional{ core_commit(*found->Base) } : std::nullopt,
				};
			}

			[[nodiscard]] auto read_committer_time_seconds(
				const core::git_commit_oid aCommit,
				const std::stop_token      aStop)
				-> application::git_cache_result<std::optional<std::int64_t>> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				const auto commit = process_commit(aCommit);
				if (!commit || aCommit.algorithm() != State->Request.ObjectFormat) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				const auto found = State->Process->read_committer_time_seconds(
					{ .Repository = State->Repository, .Commit = *commit }, aStop);
				if (!State->active_boundary_still_owned()) {
					if (!found && found.error() == application::git_process_failure::reap_failure) {
						static_cast<void>(State->process_failure(found.error()));
					}
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				if (!found) {
					return std::unexpected{ State->process_failure(found.error()) };
				}
				return *found;
			}

			[[nodiscard]] auto materialize_agent_workspace(
				const application::agent_workspace_materialization_request& aRequest,
				const std::stop_token                                       aStop)
				-> application::git_cache_result<
					application::owned_agent_workspace> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				if (
					!application::valid(aRequest)
					|| aRequest.ApplicationSession
						!= State->Request.ApplicationSession
					|| aRequest.Authority != State->Request.Authority
					|| aRequest.Session.Binding.HeadOid.algorithm()
						!= State->Request.ObjectFormat) {
					return std::unexpected{
						application::git_cache_failure::invalid_authority
					};
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto materialized = State->Process->materialize_agent_workspace(
					application::git_agent_workspace_request{
						.Repository      = State->Repository,
						.Materialization = aRequest,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (materialized.has_value()) {
						static_cast<void>(State->Process->cleanup_agent_workspace(
							*materialized,
							{ }));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!materialized.has_value()) {
					return std::unexpected{
						State->process_failure(materialized.error())
					};
				}
				return *materialized;
			}

			[[nodiscard]] auto cleanup_agent_workspace(
				const application::owned_agent_workspace& aWorkspace,
				const std::stop_token                     aStop)
				-> application::git_cache_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				if (
					!application::valid(aWorkspace)
					|| aWorkspace.ApplicationSession
						!= State->Request.ApplicationSession
					|| aWorkspace.Authority != State->Request.Authority
					|| aWorkspace.Workspace.PinnedHead->algorithm()
						!= State->Request.ObjectFormat
					|| !State->validated_inventory(aStop, false).has_value()) {
					return std::unexpected{
						application::git_cache_failure::invalid_authority
					};
				}
				const auto cleaned = State->Process->cleanup_agent_workspace(
					aWorkspace,
					aStop);
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!cleaned.has_value()) {
					return std::unexpected{
						State->process_failure(cleaned.error())
					};
				}
				return { };
			}

			[[nodiscard]] auto materialize_blob(
				const application::git_cache_blob_request& aRequest,
				const std::stop_token                      aStop)
				-> application::git_cache_result<std::vector<std::uint8_t>> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				const auto expected = process_blob(aRequest.Blob);
				if (
					!expected.has_value()
					|| aRequest.Blob.algorithm() != State->Request.ObjectFormat
					|| aRequest.MaximumBytes == 0U
					|| aRequest.MaximumBytes
						> application::maximum_git_cache_blob_materialization_bytes) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				auto collector    = blob_collector{ };
				auto materialized = State->Process->materialize_blob(
					application::git_blob_request{
						.Repository   = State->Repository,
						.Blob         = *expected,
						.MaximumBytes = aRequest.MaximumBytes,
					},
					collector,
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!materialized.has_value()
						&& materialized.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(State->process_failure(materialized.error()));
					}
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				if (!materialized.has_value()) {
					if (
						materialized.error()
							== application::git_process_failure::object_unavailable
						|| materialized.error()
							== application::git_process_failure::nonzero_exit) {
						return std::unexpected{ application::git_cache_failure::object_mismatch };
					}
					if (
						materialized.error()
						== application::git_process_failure::standard_output_limit) {
						return std::unexpected{ application::git_cache_failure::quota_exceeded };
					}
					return std::unexpected{ State->process_failure(materialized.error()) };
				}
				if (collector.Blob != expected) {
					return std::unexpected{ application::git_cache_failure::object_mismatch };
				}
				return std::move(collector.Bytes);
			}

			[[nodiscard]] auto find_blob_at_path(
				const application::git_cache_tree_entry_request& aRequest,
				const std::stop_token                            aStop)
				-> application::git_cache_result<
					std::optional<core::git_blob_oid>> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				const auto expected = process_commit(aRequest.Commit);
				const auto path     = aRequest.Path.generic_u8string();
				if (!expected.has_value()
					|| aRequest.Commit.algorithm() != State->Request.ObjectFormat
					|| path.empty() || aRequest.Path.is_absolute()
					|| path.size() > application::maximum_git_tree_entry_path_bytes
					|| path.find(u8'\0') != decltype(path)::npos
					|| std::ranges::any_of(
						aRequest.Path,
						[](const auto& aComponent) {
							return aComponent == "." || aComponent == "..";
						})) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				const auto found = State->Process->find_blob_at_path(
					application::git_tree_entry_request{
						.Repository = State->Repository,
						.Commit     = *expected,
						.Path       = aRequest.Path,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (!found.has_value()
						&& found.error() == application::git_process_failure::reap_failure) {
						static_cast<void>(State->process_failure(found.error()));
					}
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				if (!found.has_value()) {
					return std::unexpected{ State->process_failure(found.error()) };
				}
				return found->has_value()
					? std::optional{ core_blob(**found) }
					: std::optional<core::git_blob_oid>{ };
			}

			[[nodiscard]] auto find_rename_pairs(
				const application::git_cache_rename_pair_request& aRequest,
				const std::stop_token                             aStop)
				-> application::git_cache_result<
					application::git_rename_pair_observation> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{ application::git_cache_failure::cancelled };
				}
				const auto left      = process_commit(aRequest.Left);
				const auto right     = process_commit(aRequest.Right);
				const auto validPath = [](const std::filesystem::path& aPath) {
					const auto path = aPath.generic_u8string();
					return !path.empty() && !aPath.is_absolute()
						&& path.size() <= application::maximum_git_tree_entry_path_bytes
						&& path.find(u8'\0') == decltype(path)::npos
						&& std::ranges::none_of(
							aPath,
							[](const auto& aComponent) {
								return aComponent == "." || aComponent == "..";
							});
				};
				if (!left.has_value() || !right.has_value()
					|| aRequest.Left.algorithm() != State->Request.ObjectFormat
					|| aRequest.Right.algorithm() != State->Request.ObjectFormat
					|| !validPath(aRequest.CurrentPath)
					|| (aRequest.PreviousPath.has_value()
						&& !validPath(*aRequest.PreviousPath))) {
					return std::unexpected{ application::git_cache_failure::invalid_request };
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{ application::git_cache_failure::invalid_repository };
				}
				const auto found = State->Process->find_rename_pairs(
					application::git_rename_pair_request{
						.Repository   = State->Repository,
						.Left         = *left,
						.Right        = *right,
						.CurrentPath  = aRequest.CurrentPath,
						.PreviousPath = aRequest.PreviousPath,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (!found.has_value()
						&& found.error() == application::git_process_failure::reap_failure) {
						static_cast<void>(State->process_failure(found.error()));
					}
					return std::unexpected{ application::git_cache_failure::repair_required };
				}
				if (!found.has_value()) {
					return std::unexpected{ State->process_failure(found.error()) };
				}
				return *found;
			}

			[[nodiscard]] auto prune_retained(
				const application::git_ref_prune_authorization&
									  aAuthorization,
				const std::stop_token aStop)
				-> application::git_cache_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (
					!application::valid(aAuthorization)
					|| aAuthorization.Expected.Scope
						!= State->Request.Authority.Scope
					|| aAuthorization.Expected.Base.algorithm()
						!= State->Request.ObjectFormat) {
					return std::unexpected{
						application::git_cache_failure::
							authorization_mismatch
					};
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				const auto baseRef = retained_ref(aAuthorization.Expected.Scope, aAuthorization.Expected.Ordinal,
												  application::git_private_ref_endpoint::base);
				const auto headRef = retained_ref(aAuthorization.Expected.Scope, aAuthorization.Expected.Ordinal,
												  application::git_private_ref_endpoint::head);
				const auto pair    = inspect_pair_optional(baseRef, headRef, aStop);
				if (!pair.has_value()) {
					return std::unexpected{ pair.error() };
				}
				if (!pair->first.has_value() && !pair->second.has_value()) {
					return { };
				}
				const auto base = process_commit(aAuthorization.Expected.Base);
				const auto head = process_commit(aAuthorization.Expected.Head);
				if (!base.has_value() || !head.has_value() || !pair->first.has_value() || !pair->second.has_value() || *pair->first != *base
					|| *pair->second != *head) {
					return std::unexpected{ application::git_cache_failure::ref_conflict };
				}
				const std::array<application::git_ref_mutation, 2U>
					mutations{
						application::git_ref_remove{
							.Ref         = baseRef,
							.ExpectedOld = *base,
						},
						application::git_ref_remove{
							.Ref         = headRef,
							.ExpectedOld = *head,
						},
					};
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto pruned = State->Process->update_refs(
					application::git_update_refs_request{
						.Repository = State->Repository,
						.Mutations  = mutations,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!pruned.has_value()
						&& pruned.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(pruned.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				return pruned.has_value()
					? application::git_cache_result<void>{ }
					: std::unexpected{
						  State->process_failure(pruned.error())
					  };
			}

			[[nodiscard]] auto abort_operation(
				const core::epoch_operation_id aOperation,
				const std::stop_token          aStop)
				-> application::git_cache_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!aOperation.valid()) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				const auto inventory = State->validated_inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				if (!State->mark_operation_aborted(aOperation)) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				const auto pair = inspect_pair_optional(
					staging_ref(
						aOperation,
						application::git_private_ref_endpoint::base),
					staging_ref(
						aOperation,
						application::git_private_ref_endpoint::head),
					aStop);
				if (!pair.has_value()) {
					return std::unexpected{ pair.error() };
				}
				if (
					pair->first.has_value() != pair->second.has_value()) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (pair->first.has_value()) {
					const std::array<application::git_ref_mutation, 2U>
						mutations{
							application::git_ref_remove{
								.Ref = staging_ref(
									aOperation,
									application::git_private_ref_endpoint::base),
								.ExpectedOld = *pair->first,
							},
							application::git_ref_remove{
								.Ref = staging_ref(
									aOperation,
									application::git_private_ref_endpoint::head),
								.ExpectedOld = *pair->second,
							},
						};
					if (!State->active_boundary_still_owned()) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					const auto removed = State->Process->update_refs(
						application::git_update_refs_request{
							.Repository = State->Repository,
							.Mutations  = mutations,
						},
						aStop);
					if (!State->active_boundary_still_owned()) {
						if (
							!removed.has_value()
							&& removed.error()
								== application::git_process_failure::
									reap_failure) {
							static_cast<void>(
								State->process_failure(removed.error()));
						}
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					if (!removed.has_value()) {
						return std::unexpected{
							State->process_failure(removed.error())
						};
					}
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				const auto spool = State->Process->pack_spool_path(
					process_operation(aOperation));
				if (!spool.has_value()) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				const auto receiving = spool->parent_path()
					/ ("." + lower_hex(aOperation.bytes())
					   + ".pack.receiving");
				const auto record = State->find_spool(aOperation);
				if (!record.has_value()) {
					if (!missing(*spool) || !missing(receiving)) {
						return std::unexpected{
							application::git_cache_failure::spool_failure
						};
					}
					State->forget_operation(aOperation);
					return { };
				}
				if (
					record->Completed != *spool
					|| record->Receiving != receiving
					|| (!missing(*spool)
						&& !remove_exact_private_file(
							*spool,
							record->Identity))
					|| (!missing(receiving)
						&& !remove_exact_private_file(
							receiving,
							record->Identity))) {
					return std::unexpected{
						application::git_cache_failure::spool_failure
					};
				}
				State->forget_operation(aOperation);
				return { };
			}

			[[nodiscard]] auto inspect_operation_recovery(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop)
				-> application::git_cache_result<
					application::git_operation_recovery_observation> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				return inspect_operation_recovery_locked(aRequest, aStop);
			}

			[[nodiscard]] auto abort_recovered_operation(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop)
				-> application::git_cache_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				const auto      observed = inspect_operation_recovery_locked(aRequest, aStop);
				if (!observed.has_value()) {
					return std::unexpected{ observed.error() };
				}
				const auto preserveUnownedEmpty = observed->Spool == application::git_recovery_match::unowned_empty
					&& !aRequest.Receiving && !aRequest.Spool && !aRequest.Install
					&& observed->Refs == application::git_recovery_ref_topology::absent;
				const auto spoolSafe = observed->Spool == application::git_recovery_match::absent
					|| observed->Spool == application::git_recovery_match::exact || preserveUnownedEmpty;
				const auto stagingSafe = observed->Staging == application::git_recovery_match::absent
					|| observed->Staging == application::git_recovery_match::exact;
				const auto expectedPackPreexisting = aRequest.Spool
					&& aRequest.Spool->ExpectedPackPreexisting;
				const auto retainUnrecordedInstall = !aRequest.Install && aRequest.Spool
					&& !expectedPackPreexisting
					&& observed->Installed == application::git_recovery_match::inventory_only
					&& observed->Refs == application::git_recovery_ref_topology::absent;
				const auto installSafe     = aRequest.Install
					? observed->Installed
						== application::git_recovery_match::exact
					: expectedPackPreexisting
					? observed->Installed
						== application::git_recovery_match::exact
					: observed->Installed == application::git_recovery_match::absent
						|| retainUnrecordedInstall;
				const auto requiresObjects = observed->Installed == application::git_recovery_match::exact
					|| retainUnrecordedInstall
					|| observed->Staging == application::git_recovery_match::exact;
				const auto objectsSafe = requiresObjects
					? observed->Objects
						== application::git_recovery_object_match::exact
					: observed->Objects
							== application::git_recovery_object_match::not_checked
						|| observed->Objects
							== application::git_recovery_object_match::exact;
				if (
					observed->Final != application::git_recovery_match::absent
					|| !spoolSafe || !stagingSafe || !installSafe
					|| !objectsSafe
					|| (observed->Staging == application::git_recovery_match::exact
						&& !aRequest.Install && !expectedPackPreexisting)
					|| (observed->Refs
							!= application::git_recovery_ref_topology::absent
						&& observed->Refs
							!= application::git_recovery_ref_topology::staging)) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!State->mark_operation_aborted(aRequest.Operation)) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				const auto stagingRemoved = observed->Staging == application::git_recovery_match::exact
					? remove_exact_recovery_staging(aRequest, aStop)
					: application::git_cache_result<void>{ };
				if (!stagingRemoved.has_value()) {
					return std::unexpected{ stagingRemoved.error() };
				}
				const auto spoolRemoved = observed->Spool == application::git_recovery_match::exact
					? remove_exact_recovery_spool(aRequest, aStop)
					: application::git_cache_result<void>{ };
				if (!spoolRemoved.has_value()) {
					return std::unexpected{ spoolRemoved.error() };
				}
				State->forget_operation(aRequest.Operation);
				const auto reconciled = inspect_operation_recovery_locked(aRequest, aStop);
				if (
					!reconciled.has_value()
					|| (reconciled->Spool != application::git_recovery_match::absent
						&& !(preserveUnownedEmpty && reconciled->Spool == application::git_recovery_match::unowned_empty))
					|| reconciled->Staging
						!= application::git_recovery_match::absent
					|| reconciled->Final
						!= application::git_recovery_match::absent
					|| reconciled->Refs
						!= application::git_recovery_ref_topology::absent) {
					return std::unexpected{
						reconciled.has_value()
							? application::git_cache_failure::repair_required
							: reconciled.error()
					};
				}
				return { };
			}

			[[nodiscard]] auto complete_recovered_operation(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop)
				-> application::git_cache_result<void> override
			{
				std::lock_guard operationGuard{ State->OperationMutex };
				const auto      observed = inspect_operation_recovery_locked(aRequest, aStop);
				if (!observed.has_value()) {
					return std::unexpected{ observed.error() };
				}
				if (
					!aRequest.Install
					|| observed->Installed
						!= application::git_recovery_match::exact
					|| observed->Final != application::git_recovery_match::exact
					|| observed->Objects
						!= application::git_recovery_object_match::exact
					|| (observed->Spool != application::git_recovery_match::absent
						&& observed->Spool
							!= application::git_recovery_match::exact)
					|| (observed->Staging
							!= application::git_recovery_match::absent
						&& observed->Staging
							!= application::git_recovery_match::exact)) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				const auto stagingRemoved = observed->Staging == application::git_recovery_match::exact
					? remove_exact_recovery_staging(aRequest, aStop)
					: application::git_cache_result<void>{ };
				if (!stagingRemoved.has_value()) {
					return std::unexpected{ stagingRemoved.error() };
				}
				const auto spoolRemoved = observed->Spool == application::git_recovery_match::exact
					? remove_exact_recovery_spool(aRequest, aStop)
					: application::git_cache_result<void>{ };
				if (!spoolRemoved.has_value()) {
					return std::unexpected{ spoolRemoved.error() };
				}
				State->forget_operation(aRequest.Operation);
				const auto reconciled = inspect_operation_recovery_locked(aRequest, aStop);
				if (
					!reconciled.has_value()
					|| reconciled->Spool
						!= application::git_recovery_match::absent
					|| reconciled->Staging
						!= application::git_recovery_match::absent
					|| reconciled->Final
						!= application::git_recovery_match::exact
					|| reconciled->Installed
						!= application::git_recovery_match::exact
					|| reconciled->Objects
						!= application::git_recovery_object_match::exact
					|| reconciled->Refs
						!= application::git_recovery_ref_topology::final) {
					return std::unexpected{
						reconciled.has_value()
							? application::git_cache_failure::repair_required
							: reconciled.error()
					};
				}
				return { };
			}

		private:
			[[nodiscard]] auto inspect_operation_recovery_locked(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop)
				-> application::git_cache_result<
					application::git_operation_recovery_observation>
			{
				using application::git_recovery_match;

				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				if (
					aRequest.Scope != State->Request.Authority.Scope
					|| aRequest.Base.algorithm() != State->Request.ObjectFormat) {
					return std::unexpected{
						application::git_cache_failure::authorization_mismatch
					};
				}
				if (!State->validate_topology(aStop)) {
					return std::unexpected{
						aStop.stop_requested()
							? application::git_cache_failure::cancelled
							: application::git_cache_failure::invalid_repository
					};
				}

				auto result = application::git_operation_recovery_observation{
					.Operation = aRequest.Operation,
				};
				const auto spoolPath = State->Process->pack_spool_path(
					process_operation(aRequest.Operation));
				if (!spoolPath.has_value()) {
					return std::unexpected{
						State->process_failure(spoolPath.error())
					};
				}
				const auto receivingPath = spoolPath->parent_path()
					/ ("." + lower_hex(aRequest.Operation.bytes())
					   + ".pack.receiving");
				const auto completedSpool = inspect_recovery_file(*spoolPath);
				const auto receivingSpool = inspect_recovery_file(receivingPath);
				if (
					completedSpool.Kind == recovery_file_kind::absent
					&& receivingSpool.Kind == recovery_file_kind::absent) {
					result.Spool = git_recovery_match::absent;
				} else if (!aRequest.Receiving && !aRequest.Spool && !aRequest.Install
						   && completedSpool.Kind == recovery_file_kind::absent
						   && receivingSpool.Kind == recovery_file_kind::private_regular
						   && receivingSpool.Bytes == 0U) {
					result.Spool = git_recovery_match::unowned_empty;
				} else if (aRequest.Receiving && !aRequest.Spool) {
					const auto identity = exact_identity(aRequest.Receiving->Identity);
					const auto owned    = [&](const recovery_file_observation& aFile) {
						return aFile.Kind == recovery_file_kind::absent
							|| (aFile.Kind == recovery_file_kind::private_regular && identity
								&& aFile.Identity == identity && aFile.Bytes <= aRequest.Receiving->MaximumBytes);
					};
					result.Spool = owned(receivingSpool) && owned(completedSpool)
						? git_recovery_match::exact
						: git_recovery_match::mismatch;
				} else if (
					receivingSpool.Kind != recovery_file_kind::absent
					|| (completedSpool.Kind == recovery_file_kind::private_regular
						&& !aRequest.Spool)) {
					result.Spool = git_recovery_match::ambiguous;
				} else if (
					completedSpool.Kind != recovery_file_kind::private_regular) {
					result.Spool = git_recovery_match::mismatch;
				} else {
					const auto expected = exact_identity(
						aRequest.Spool->CompletedSpool.Identity);
					result.Spool = expected.has_value()
							&& completedSpool.Identity == expected
							&& completedSpool.Bytes
								== aRequest.Spool->CompletedSpool.Bytes
						? git_recovery_match::exact
						: git_recovery_match::mismatch;
				}

				auto expectedPack = std::optional<application::git_pack_oid>{ };
				if (aRequest.Install) {
					expectedPack = aRequest.Install->Pack;
				} else if (aRequest.Spool) {
					expectedPack = aRequest.Spool->ExpectedPack;
				}
				if (expectedPack) {
					const auto paths = paths_for_installed_pack(
						State->Repository,
						*expectedPack);
					if (!paths.has_value()) {
						return std::unexpected{
							application::git_cache_failure::invalid_request
						};
					}
					const auto packFile    = inspect_recovery_file(paths->Pack);
					const auto indexFile   = inspect_recovery_file(paths->Index);
					const auto reverseFile = inspect_recovery_file(paths->ReverseIndex);
					if (
						packFile.Kind == recovery_file_kind::absent
						&& indexFile.Kind == recovery_file_kind::absent
						&& reverseFile.Kind == recovery_file_kind::absent) {
						result.Installed = git_recovery_match::absent;
					} else if (
						reverseFile.Kind != recovery_file_kind::absent
						|| (packFile.Kind == recovery_file_kind::absent
							&& indexFile.Kind
								!= recovery_file_kind::absent)
						|| (packFile.Kind != recovery_file_kind::absent
							&& indexFile.Kind
								== recovery_file_kind::absent)) {
						result.Installed = git_recovery_match::ambiguous;
					} else if (
						packFile.Kind != recovery_file_kind::private_regular
						|| indexFile.Kind
							!= recovery_file_kind::private_regular) {
						result.Installed = git_recovery_match::mismatch;
					} else {
						const auto inventory = State->validated_inventory(aStop, false);
						if (!inventory.has_value()) {
							return std::unexpected{ inventory.error() };
						}
						const auto entry = std::ranges::find_if(
							inventory->Packs,
							[&](const auto& aCandidate) {
								return aCandidate.Pack == *expectedPack;
							});
						if (entry == inventory->Packs.end()) {
							result.Installed = git_recovery_match::mismatch;
						} else if (aRequest.Install) {
							const auto expectedPackIdentity = exact_identity(
								aRequest.Install->InstalledPack.Identity);
							const auto expectedIndexIdentity = exact_identity(
								aRequest.Install->InstalledIndex.Identity);
							auto inventoryAgrees = true;
							if (aRequest.Spool) {
								auto expectedInventory = aRequest.Spool->PreimportPacks;
								if (!aRequest.Spool->ExpectedPackPreexisting) {
									const auto position = std::ranges::lower_bound(
										expectedInventory,
										*expectedPack,
										{ },
										&application::git_pack_inventory_entry::Pack);
									expectedInventory.insert(
										position,
										application::git_pack_inventory_entry{
											.Pack       = *expectedPack,
											.PackBytes  = aRequest.Install
															  ->InstalledPack.Bytes,
											.IndexBytes = aRequest.Install
															  ->InstalledIndex.Bytes,
										});
								}
								inventoryAgrees = inventory->Packs == expectedInventory;
							}
							result.Installed = expectedPackIdentity.has_value()
									&& expectedIndexIdentity.has_value()
									&& packFile.Identity
										== expectedPackIdentity
									&& indexFile.Identity
										== expectedIndexIdentity
									&& packFile.Bytes
										== aRequest.Install->InstalledPack.Bytes
									&& indexFile.Bytes
										== aRequest.Install->InstalledIndex.Bytes
									&& entry->PackBytes == packFile.Bytes
									&& entry->IndexBytes == indexFile.Bytes
									&& inventoryAgrees
								? git_recovery_match::exact
								: git_recovery_match::mismatch;
						} else if (aRequest.Spool->ExpectedPackPreexisting) {
							const auto prior = std::ranges::find_if(
								aRequest.Spool->PreimportPacks,
								[&](const auto& aCandidate) {
									return aCandidate.Pack == *expectedPack;
								});
							result.Installed = prior != aRequest.Spool->PreimportPacks.end()
									&& inventory->Packs
										== aRequest.Spool->PreimportPacks
									&& prior->PackBytes == packFile.Bytes
									&& prior->IndexBytes == indexFile.Bytes
								? git_recovery_match::exact
								: git_recovery_match::mismatch;
						} else {
							const auto priorUnchanged = std::ranges::all_of(
								aRequest.Spool->PreimportPacks,
								[&](const auto& aPrior) {
									return std::ranges::find(
											   inventory->Packs,
											   aPrior)
										!= inventory->Packs.end();
								});
							result.Installed = priorUnchanged
									&& entry->PackBytes == aRequest.Spool->CompletedSpool.Bytes
									&& inventory->Packs.size()
										== aRequest.Spool->PreimportPacks.size()
											+ 1U
								? git_recovery_match::inventory_only
								: git_recovery_match::mismatch;
						}
					}
				}

				const auto base = process_commit(aRequest.Base);
				const auto head = process_commit(aRequest.Head);
				if (!base.has_value() || !head.has_value()) {
					return std::unexpected{
						application::git_cache_failure::object_mismatch
					};
				}
				const auto staging = inspect_pair_target_optional(
					staging_ref(
						aRequest.Operation,
						application::git_private_ref_endpoint::base),
					staging_ref(
						aRequest.Operation,
						application::git_private_ref_endpoint::head),
					aStop);
				if (!staging.has_value()) {
					return std::unexpected{ staging.error() };
				}
				const auto final = inspect_pair_target_optional(
					retained_ref(
						aRequest.Scope,
						aRequest.Ordinal,
						application::git_private_ref_endpoint::base),
					retained_ref(
						aRequest.Scope,
						aRequest.Ordinal,
						application::git_private_ref_endpoint::head),
					aStop);
				if (!final.has_value()) {
					return std::unexpected{ final.error() };
				}
				result.Staging = classify_recovery_ref_pair(
					*staging,
					*base,
					*head);
				result.Final = classify_recovery_ref_pair(
					*final,
					*base,
					*head);
				result.Refs = classify_recovery_ref_topology(
					result.Staging,
					result.Final);

				const auto checkObjects = result.Installed == git_recovery_match::exact
					|| result.Installed == git_recovery_match::inventory_only
					|| result.Staging == git_recovery_match::exact
					|| result.Final == git_recovery_match::exact;
				if (checkObjects) {
					const auto objects = inspect_recovery_objects(
						aRequest,
						*base,
						*head,
						aStop);
					if (!objects.has_value()) {
						return std::unexpected{ objects.error() };
					}
					result.Objects = *objects;
				}
				return result;
			}

			[[nodiscard]] auto recovery_object_failure(
				const application::git_process_error& aFailure)
				-> application::git_cache_result<
					application::git_recovery_object_match>
			{
				if (
					aFailure
					== application::git_process_failure::object_unavailable) {
					return application::git_recovery_object_match::unavailable;
				}
				if (aFailure == application::git_process_failure::nonzero_exit) {
					return application::git_recovery_object_match::mismatch;
				}
				return std::unexpected{ State->process_failure(aFailure) };
			}

			[[nodiscard]] auto inspect_recovery_objects(
				const application::git_operation_recovery_request& aRequest,
				const application::git_commit_id&                  aBase,
				const application::git_commit_id&                  aHead,
				const std::stop_token                              aStop)
				-> application::git_cache_result<
					application::git_recovery_object_match>
			{
				using application::git_process_failure;
				using application::git_recovery_object_match;
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::invalid_repository
					};
				}
				const auto baseInspection = State->Process->inspect_commit(
					{ .Repository = State->Repository, .Commit = aBase },
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!baseInspection.has_value()
						&& baseInspection.error()
							== git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(baseInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!baseInspection.has_value()) {
					return recovery_object_failure(baseInspection.error());
				}
				const auto headInspection = State->Process->inspect_commit(
					{ .Repository = State->Repository, .Commit = aHead },
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!headInspection.has_value()
						&& headInspection.error()
							== git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(headInspection.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!headInspection.has_value()) {
					return recovery_object_failure(headInspection.error());
				}
				if (aRequest.Install) {
					if (
						core_tree(baseInspection->Tree)
							!= aRequest.Install->BaseTree
						|| core_tree(headInspection->Tree)
							!= aRequest.Install->HeadTree) {
						return git_recovery_object_match::mismatch;
					}
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::invalid_repository
					};
				}
				const auto sparse = (aRequest.Install && aRequest.Install->Residency == application::git_revision_residency::sparse_commit_trees)
					|| (aRequest.Spool && aRequest.Spool->Residency == application::git_revision_residency::sparse_commit_trees);
				auto connected = application::git_process_result<void>{ };
				if (!sparse) {
					connected = State->Process->verify_connectivity(
						{ .Repository = State->Repository, .Base = aBase, .Head = aHead }, aStop);
				}
				if (!State->active_boundary_still_owned()) {
					if (
						!connected.has_value()
						&& connected.error() == git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(connected.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!connected.has_value()) {
					return recovery_object_failure(connected.error());
				}
				return git_recovery_object_match::exact;
			}

			[[nodiscard]] auto remove_exact_recovery_staging(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop) -> application::git_cache_result<void>
			{
				const auto base = process_commit(aRequest.Base);
				const auto head = process_commit(aRequest.Head);
				if (!base.has_value() || !head.has_value()) {
					return std::unexpected{
						application::git_cache_failure::object_mismatch
					};
				}
				const std::array<application::git_ref_mutation, 2U> removals{
					application::git_ref_remove{
						.Ref = staging_ref(
							aRequest.Operation,
							application::git_private_ref_endpoint::base),
						.ExpectedOld = *base,
					},
					application::git_ref_remove{
						.Ref = staging_ref(
							aRequest.Operation,
							application::git_private_ref_endpoint::head),
						.ExpectedOld = *head,
					},
				};
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::invalid_repository
					};
				}
				const auto removed = State->Process->update_refs(
					application::git_update_refs_request{
						.Repository = State->Repository,
						.Mutations  = removals,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!removed.has_value()
						&& removed.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(removed.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				return removed.has_value()
					? application::git_cache_result<void>{ }
					: application::git_cache_result<void>{
						  std::unexpected{
							  State->process_failure(removed.error()) }
					  };
			}

			[[nodiscard]] auto remove_exact_recovery_spool(
				const application::git_operation_recovery_request& aRequest,
				const std::stop_token                              aStop)
				-> application::git_cache_result<void>
			{
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!aRequest.Spool && !aRequest.Receiving) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				const auto expected = exact_identity(
					aRequest.Spool ? aRequest.Spool->CompletedSpool.Identity : aRequest.Receiving->Identity);
				const auto path = State->Process->pack_spool_path(
					process_operation(aRequest.Operation));
				if (!expected.has_value() || !path.has_value()) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				const auto receiving = path->parent_path()
					/ ("." + lower_hex(aRequest.Operation.bytes())
					   + ".pack.receiving");
				if (
					(!missing(receiving) && (!aRequest.Receiving || !remove_exact_private_file(receiving, *expected)))
					|| (!missing(*path) && !remove_exact_private_file(*path, *expected))) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				return { };
			}

			[[nodiscard]] auto inspect_pair_target_optional(
				const application::git_private_ref& aBaseRef,
				const application::git_private_ref& aHeadRef,
				const std::stop_token               aStop)
				-> application::git_cache_result<
					std::pair<
						std::optional<application::git_commit_id>,
						std::optional<application::git_commit_id>>>
			{
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::invalid_repository
					};
				}
				const auto base = State->Process->inspect_ref_target(
					application::git_inspect_ref_request{
						.Repository = State->Repository,
						.Ref        = aBaseRef,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!base.has_value()
						&& base.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(base.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!base.has_value()) {
					return std::unexpected{
						State->process_failure(base.error())
					};
				}
				const auto head = State->Process->inspect_ref_target(
					application::git_inspect_ref_request{
						.Repository = State->Repository,
						.Ref        = aHeadRef,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!head.has_value()
						&& head.error()
							== application::git_process_failure::reap_failure) {
						static_cast<void>(
							State->process_failure(head.error()));
					}
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (!head.has_value()) {
					return std::unexpected{
						State->process_failure(head.error())
					};
				}
				return std::pair{ *base, *head };
			}

			[[nodiscard]] auto inspect_pair_optional(
				const application::git_private_ref& aBaseRef,
				const application::git_private_ref& aHeadRef,
				const std::stop_token               aStop)
				-> application::git_cache_result<
					std::pair<
						std::optional<application::git_commit_id>,
						std::optional<application::git_commit_id>>>
			{
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto base = State->Process->inspect_ref(
					application::git_inspect_ref_request{
						.Repository = State->Repository,
						.Ref        = aBaseRef,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!base.has_value()
						&& base.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(base.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!base.has_value()) {
					return std::unexpected{
						State->process_failure(base.error())
					};
				}
				if (!State->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}
				const auto head = State->Process->inspect_ref(
					application::git_inspect_ref_request{
						.Repository = State->Repository,
						.Ref        = aHeadRef,
					},
					aStop);
				if (!State->active_boundary_still_owned()) {
					if (
						!head.has_value()
						&& head.error()
							== application::git_process_failure::
								reap_failure) {
						static_cast<void>(
							State->process_failure(head.error()));
					}
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!head.has_value()) {
					return std::unexpected{
						State->process_failure(head.error())
					};
				}
				return std::pair{ *base, *head };
			}

			[[nodiscard]] auto inspect_pair(
				const application::git_private_ref& aBaseRef,
				const application::git_private_ref& aHeadRef,
				const std::stop_token               aStop)
				-> application::git_cache_result<
					std::pair<
						application::git_commit_id,
						application::git_commit_id>>
			{
				const auto pair = inspect_pair_optional(aBaseRef, aHeadRef, aStop);
				if (
					!pair.has_value() || !pair->first.has_value()
					|| !pair->second.has_value()) {
					return std::unexpected{
						pair.has_value()
							? application::git_cache_failure::ref_conflict
							: pair.error()
					};
				}
				return std::pair{ *pair->first, *pair->second };
			}

			std::shared_ptr<cache_state> State;
		};

		class installed_git_cache final : public application::git_cache_port
		{
		public:
			explicit installed_git_cache(
				installed_git_cache_configuration aConfiguration)
				: Configuration{ std::move(aConfiguration) }
			{
			}

			[[nodiscard]] auto open(
				const application::git_cache_open_request& aRequest,
				const std::stop_token                      aStop)
				-> application::git_cache_result<
					std::unique_ptr<application::git_cache_session>> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (!application::valid(aRequest)) {
					return std::unexpected{
						application::git_cache_failure::invalid_request
					};
				}
				const auto storage = canonical_existing(Configuration.StorageRoot);
				if (!storage.has_value() || !private_directory(*storage)) {
					return std::unexpected{
						application::git_cache_failure::invalid_path
					};
				}
				if (
					!make_private_directory(*storage / "locks")
					|| !make_private_directory(*storage / "locks" / "v1")) {
					return std::unexpected{
						application::git_cache_failure::invalid_path
					};
				}
				const auto cacheText  = lower_hex(aRequest.Authority.Cache.bytes());
				const auto repository = *storage / "repositories" / "v1"
					/ (cacheText + ".git");
				const auto lockPath = *storage / "locks" / "v1" / (cacheText + ".lock");
				auto       lock     = std::make_unique<cache_lock>();
				if (!lock->acquire(lockPath)) {
					return std::unexpected{
						application::git_cache_failure::lock_unavailable
					};
				}
				const auto lockIdentity         = lock->identity();
				const auto manifestLockIdentity = lock->manifest_identity();
				if (
					!lockIdentity.has_value()
					|| !manifestLockIdentity.has_value()
					|| !lock->still_names(lockPath)) {
					return std::unexpected{
						application::git_cache_failure::lock_unavailable
					};
				}
				const auto deleted = deletion_marker_present(*storage, cacheText);
				if (!deleted.has_value()) {
					return std::unexpected{ deleted.error() };
				}
				if (*deleted) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				auto repositoryAbsent   = missing(repository);
				auto repositoryIdentity = std::optional<exact_artifact_identity>{ };
				if (!repositoryAbsent) {
					repositoryIdentity = exact_identity(
						repository,
						exact_artifact_kind::directory);
					if (
						!repositoryIdentity.has_value()
						|| !private_directory(repository)) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
				}
				auto process = make_installed_git_process(
					{
						.GitExecutable              = Configuration.GitExecutable,
						.IsolationRoot              = *storage,
						.WindowsSystemRoot          = Configuration.WindowsSystemRoot,
						.WorkspaceOwnershipRegistry = Configuration.WorkspaceOwnershipRegistry,
					},
					aStop);
				if (!process.has_value()) {
					const auto failure = process.error()
							== application::git_process_failure::cancelled
						? application::git_cache_failure::cancelled
						: application::git_cache_failure::process_failure;
					return std::unexpected{ application::git_cache_error{
						failure,
						std::move(process.error().Detail) } };
				}

				auto state = std::make_shared<cache_state>(
					aRequest,
					*storage,
					repository,
					repositoryIdentity,
					lockPath,
					*lockIdentity,
					*manifestLockIdentity,
					std::move(lock),
					std::move(*process));
				if (!repositoryAbsent && state->pristine_legacy_cache_after_remount(aStop)) {
					// This cache never acquired objects or refs. Preserve its old
					// metadata under an exact no-replace rename, then initialize a
					// fresh cache. Populated/unknown layouts never enter this path.
					const auto retired = random_quarantine_path(repository, ".bpr-pristine-legacy-");
					if (!retired || !state->pristine_legacy_cache_after_remount(aStop)
						|| !rename_exact_to_new_path(repository, *retired, *repositoryIdentity)) {
						return std::unexpected{ application::git_cache_failure::repair_required };
					}
					repositoryAbsent = true;
				}
				if (repositoryAbsent) {
					const auto algorithm = process_algorithm(aRequest.ObjectFormat);
					if (!algorithm.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_request
						};
					}
					auto staging = std::filesystem::path{ };
					for (std::size_t attempt = 0U; attempt < 8U; ++attempt) {
						const auto candidate = repository.parent_path()
							/ (lower_hex(state->new_token()) + ".git");
						if (candidate != repository && missing(candidate)) {
							staging = candidate;
							break;
						}
					}
					if (staging.empty()) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					const auto initialized = state->Process->initialize_bare_repository(
						application::git_initialize_repository_request{
							.Repository = staging,
							.Algorithm  = *algorithm,
						},
						aStop);
					if (!initialized.has_value()) {
						return std::unexpected{
							state->process_failure(initialized.error())
						};
					}
					const auto stagingIdentity = exact_identity(
						staging,
						exact_artifact_kind::directory);
					if (!stagingIdentity.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					if (!state->bind_repository(staging, *stagingIdentity)) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					const auto manifest        = state->expected_manifest();
					auto       publishManifest = true;
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
					std::error_code manifestInjectionError;
					publishManifest = !std::filesystem::exists(
						*storage / "inject-cache-manifest-failure",
						manifestInjectionError);
					if (manifestInjectionError) {
						publishManifest = false;
					}
#endif
					if (
						!publishManifest
						|| !write_new_private_file(
							staging / manifest_name,
							manifest)) {
						return quarantine_and_remove_exact_tree(
								   staging,
								   *stagingIdentity)
							? application::git_cache_result<
								  std::unique_ptr<
									  application::git_cache_session>>{
								  std::unexpected{
									  application::git_cache_failure::
										  invalid_manifest }
							  }
							: application::git_cache_result<std::unique_ptr<application::git_cache_session>>{ std::unexpected{ application::git_cache_failure::repair_required } };
					}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
					std::error_code validationInjectionError;
					if (
						std::filesystem::exists(
							*storage
								/ "inject-cache-post-manifest-substitution",
							validationInjectionError)
						&& !validationInjectionError) {
						const auto displaced = staging.parent_path()
							/ ".bpr-test-displaced-cache-staging.git";
						static constexpr std::array<std::uint8_t, 29U>
							canary{
								'p',
								'o',
								's',
								't',
								'-',
								'm',
								'a',
								'n',
								'i',
								'f',
								'e',
								's',
								't',
								'-',
								'r',
								'o',
								'o',
								't',
								'-',
								'c',
								'a',
								'n',
								'a',
								'r',
								'y',
								'-',
								'd',
								'a',
								't',
							};
						if (
							!rename_exact_to_new_path(
								staging,
								displaced,
								*stagingIdentity)
							|| !make_private_directory(staging)
							|| !write_new_private_file(
								staging / "replacement-canary",
								canary)) {
							return std::unexpected{
								application::git_cache_failure::
									repair_required
							};
						}
					} else if (validationInjectionError) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
#endif
					if (!state->validate_topology()) {
						return quarantine_and_remove_exact_tree(
								   staging,
								   *stagingIdentity)
							? application::git_cache_result<
								  std::unique_ptr<
									  application::git_cache_session>>{
								  std::unexpected{
									  application::git_cache_failure::
										  invalid_manifest }
							  }
							: application::git_cache_result<std::unique_ptr<application::git_cache_session>>{ std::unexpected{ application::git_cache_failure::repair_required } };
					}
					if (aStop.stop_requested()) {
						return quarantine_and_remove_exact_tree(
								   staging,
								   *stagingIdentity)
							? application::git_cache_result<
								  std::unique_ptr<
									  application::git_cache_session>>{
								  std::unexpected{
									  application::git_cache_failure::
										  cancelled }
							  }
							: application::git_cache_result<std::unique_ptr<application::git_cache_session>>{ std::unexpected{ application::git_cache_failure::repair_required } };
					}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
					std::error_code destinationInjectionError;
					if (
						std::filesystem::exists(
							*storage
								/ "inject-cache-final-destination",
							destinationInjectionError)
						&& !destinationInjectionError) {
						static_cast<void>(
							make_private_directory(repository));
						static constexpr std::array<std::uint8_t, 6U>
							canary{ 0x63U, 0x61U, 0x6eU, 0x61U, 0x72U, 0x79U };
						static_cast<void>(write_new_private_file(
							repository / "canary",
							canary));
					}
#endif
					const auto renamed = rename_exact_to_new_path(
						staging,
						repository,
						*stagingIdentity);
					if (!renamed) {
						const auto publishedIdentity = exact_identity(
							repository,
							exact_artifact_kind::directory);
						if (
							publishedIdentity != stagingIdentity
							|| !missing(staging)
							|| !synchronize_directory(
								repository.parent_path())) {
							if (
								exact_identity(
									staging,
									exact_artifact_kind::directory)
								== stagingIdentity) {
								static_cast<void>(
									quarantine_and_remove_exact_tree(
										staging,
										*stagingIdentity));
							}
							return std::unexpected{
								application::git_cache_failure::
									repair_required
							};
						}
					}
					if (!state->bind_repository(
							repository,
							*stagingIdentity)) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
				} else if (!state->active_boundary_still_owned()) {
					return std::unexpected{
						application::git_cache_failure::
							invalid_repository
					};
				}

				if (!state->validate_topology(aStop)) {
					return std::unexpected{
						aStop.stop_requested()
							? application::git_cache_failure::cancelled
							: application::git_cache_failure::
								  invalid_manifest
					};
				}
				const auto inventory = state->inventory(aStop, false);
				if (!inventory.has_value()) {
					return std::unexpected{ inventory.error() };
				}
				return std::unique_ptr<application::git_cache_session>{
					std::make_unique<installed_git_cache_session>(
						std::move(state))
				};
			}

			[[nodiscard]] auto delete_cache(
				const application::git_cache_open_request& aExpected,
				const application::git_cache_delete_authorization&
									  aAuthorization,
				const std::stop_token aStop)
				-> application::git_cache_result<void> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{
						application::git_cache_failure::cancelled
					};
				}
				if (
					!application::valid(aExpected)
					|| !application::valid(aAuthorization)
					|| aAuthorization.ApplicationSession
						!= aExpected.ApplicationSession
					|| aAuthorization.Cache != aExpected.Authority.Cache) {
					return std::unexpected{
						application::git_cache_failure::
							authorization_mismatch
					};
				}
				const auto storage = canonical_existing(Configuration.StorageRoot);
				if (!storage.has_value() || !private_directory(*storage)) {
					return std::unexpected{
						application::git_cache_failure::invalid_path
					};
				}
				if (
					!make_private_directory(*storage / "locks")
					|| !make_private_directory(*storage / "locks" / "v1")
					|| !make_private_directory(*storage / "quarantine")
					|| !make_private_directory(
						*storage / "quarantine" / "v1")) {
					return std::unexpected{
						application::git_cache_failure::invalid_path
					};
				}
				const auto cacheText  = lower_hex(aExpected.Authority.Cache.bytes());
				const auto repository = *storage / "repositories" / "v1"
					/ (cacheText + ".git");
				const auto lockPath       = *storage / "locks" / "v1" / (cacheText + ".lock");
				const auto quarantineRoot = *storage / "quarantine" / "v1";
				const auto quarantine     = quarantineRoot
					/ (cacheText + "-"
					   + lower_hex(aAuthorization.DurableIntentToken)
					   + ".git");
				const auto tombstone = quarantineRoot / (cacheText + ".deleted");
				auto       lock      = std::make_unique<cache_lock>();
				if (!lock->acquire(lockPath)) {
					return std::unexpected{
						application::git_cache_failure::lock_unavailable
					};
				}
				const auto lockIdentity         = lock->identity();
				const auto manifestLockIdentity = lock->manifest_identity();
				if (
					!lockIdentity.has_value()
					|| !manifestLockIdentity.has_value()
					|| !lock->still_names(lockPath)) {
					return std::unexpected{
						application::git_cache_failure::lock_unavailable
					};
				}
				const auto heldLockStillNames = [&] {
					return lock->identity() == lockIdentity
						&& lock->still_names(lockPath);
				};
				const auto tombstoneMissing = missing(tombstone);
				auto       tombstoneRecord  = std::optional<deletion_tombstone_record>{ };
				if (!tombstoneMissing) {
					const auto placeholder = deletion_tombstone_bytes(
						aExpected,
						aAuthorization,
						deletion_tombstone_record{
							.Repository = {
								.Kind = exact_artifact_kind::directory,
							},
							.Lock = {
								.Kind = exact_artifact_kind::regular_file,
							},
						});
					const auto actual = read_private_file(
						tombstone,
						placeholder.size());
					if (!actual.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								authorization_mismatch
						};
					}
					tombstoneRecord = parse_deletion_tombstone(
						*actual,
						aExpected,
						aAuthorization);
					if (!tombstoneRecord.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								authorization_mismatch
						};
					}
					if (tombstoneRecord->Lock != *lockIdentity) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
				}
				const auto repositoryPresent = private_directory(repository);
				const auto quarantinePresent = private_directory(quarantine);
				if (
					(repositoryPresent && quarantinePresent)
					|| (!repositoryPresent && !missing(repository))
					|| (!quarantinePresent && !missing(quarantine))
					|| (tombstoneMissing && quarantinePresent)) {
					return std::unexpected{
						application::git_cache_failure::
							repair_required
					};
				}
				if (!repositoryPresent && !quarantinePresent) {
					if (tombstoneMissing) {
						return std::unexpected{
							application::git_cache_failure::
								invalid_repository
						};
					}
					return heldLockStillNames()
						? application::git_cache_result<void>{ }
						: application::git_cache_result<void>{
							  std::unexpected{
								  application::git_cache_failure::
									  repair_required }
						  };
				}

				if (tombstoneMissing) {
					const auto validatedPath      = repositoryPresent ? repository : quarantine;
					const auto repositoryIdentity = exact_identity(
						validatedPath,
						exact_artifact_kind::directory);
					if (!repositoryIdentity.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					auto state = std::make_shared<cache_state>(
						aExpected,
						*storage,
						validatedPath,
						*repositoryIdentity,
						lockPath,
						*lockIdentity,
						*manifestLockIdentity,
						nullptr,
						nullptr);
					const auto inventory = state->validated_inventory(aStop, false);
					if (!inventory.has_value()) {
						return std::unexpected{ inventory.error() };
					}
					if (
						exact_identity(
							validatedPath,
							exact_artifact_kind::directory)
						!= repositoryIdentity) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
					tombstoneRecord = deletion_tombstone_record{
						.Repository = *repositoryIdentity,
						.Lock       = *lockIdentity,
					};
					const auto expectedTombstone = deletion_tombstone_bytes(
						aExpected,
						aAuthorization,
						*tombstoneRecord);
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
					std::error_code lockSubstitutionInjectionError;
					if (
						std::filesystem::exists(
							*storage
								/ "inject-cache-lock-substitution-during-delete",
							lockSubstitutionInjectionError)
						&& !lockSubstitutionInjectionError) {
						const auto displacedLock = lockPath.parent_path()
							/ (".bpr-test-original-delete-"
							   + lockPath.filename().string());
						static constexpr std::array<std::uint8_t, 30U>
							canary{
								'r',
								'e',
								'p',
								'l',
								'a',
								'c',
								'e',
								'm',
								'e',
								'n',
								't',
								'-',
								'd',
								'e',
								'l',
								'e',
								't',
								'e',
								'-',
								'l',
								'o',
								'c',
								'k',
								'-',
								'c',
								'a',
								'n',
								'a',
								'r',
								'y',
							};
						if (
							!rename_exact_to_new_path(
								lockPath,
								displacedLock,
								*lockIdentity)
							|| !write_new_private_file(
								lockPath,
								canary)) {
							return std::unexpected{
								application::git_cache_failure::
									repair_required
							};
						}
					} else if (lockSubstitutionInjectionError) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
#endif
					if (
						!heldLockStillNames()
						|| !write_new_private_file(
							tombstone,
							expectedTombstone)
						|| !synchronize_directory(quarantineRoot)) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
				}
				if (!tombstoneRecord.has_value()) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				const auto activePath = repositoryPresent ? repository : quarantine;
				if (
					exact_identity(
						activePath,
						exact_artifact_kind::directory)
					!= tombstoneRecord->Repository) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (repositoryPresent && !tombstoneMissing) {
					auto state = std::make_shared<cache_state>(
						aExpected,
						*storage,
						repository,
						tombstoneRecord->Repository,
						lockPath,
						*lockIdentity,
						*manifestLockIdentity,
						nullptr,
						nullptr);
					const auto inventory = state->validated_inventory({ }, false);
					if (!inventory.has_value()) {
						return std::unexpected{
							application::git_cache_failure::
								repair_required
						};
					}
				}
#if defined(BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION)
				std::error_code deleteDestinationInjectionError;
				if (
					repositoryPresent
					&& std::filesystem::exists(
						*storage
							/ "inject-cache-delete-destination-race",
						deleteDestinationInjectionError)
					&& !deleteDestinationInjectionError) {
					static_cast<void>(make_private_directory(quarantine));
					static constexpr std::array<std::uint8_t, 6U>
						canary{ 0x63U, 0x61U, 0x6eU, 0x61U, 0x72U, 0x79U };
					static_cast<void>(write_new_private_file(
						quarantine / "canary",
						canary));
				}
#endif
				if (
					!heldLockStillNames()
					|| (repositoryPresent
						&& !rename_exact_to_new_path(
							repository,
							quarantine,
							tombstoneRecord->Repository)
						&& !(
							missing(repository)
							&& exact_identity(
								   quarantine,
								   exact_artifact_kind::directory)
								== tombstoneRecord->Repository
							&& synchronize_directory(quarantineRoot)))) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				if (
					!heldLockStillNames()
					|| !remove_tree_no_follow(
						quarantine,
						tombstoneRecord->Repository,
						*storage)
					|| !synchronize_directory(quarantine.parent_path())) {
					return std::unexpected{
						application::git_cache_failure::repair_required
					};
				}
				return heldLockStillNames()
					? application::git_cache_result<void>{ }
					: application::git_cache_result<void>{
						  std::unexpected{
							  application::git_cache_failure::
								  repair_required }
					  };
			}

		private:
			installed_git_cache_configuration Configuration;
		};
	} // namespace

	auto make_installed_git_cache(
		const installed_git_cache_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::git_cache_port>,
			application::git_cache_error>
	{
		const auto root       = canonical_existing(aConfiguration.StorageRoot);
		const auto executable = canonical_existing(aConfiguration.GitExecutable);
		if (
			!root.has_value() || !executable.has_value()
			|| !private_directory(*root)) {
			return std::unexpected{
				application::git_cache_failure::invalid_path
			};
		}
		return std::unique_ptr<application::git_cache_port>{
			std::make_unique<installed_git_cache>(aConfiguration)
		};
	}
} // namespace bpr::adapters::git
