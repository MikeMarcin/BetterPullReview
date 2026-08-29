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
#include <charconv>
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

namespace bpr::adapters::git {
	namespace {
		using application::git_ancestry;
		using application::git_commit_id;
		using application::git_hash_algorithm;
		using application::git_pack_id;
		using application::git_process_error;
		using application::git_process_failure;
		using application::git_process_result;
		using application::git_tree_id;
		using clock = std::chrono::steady_clock;

		inline constexpr auto             wait_slice                  = std::chrono::milliseconds{ 10 };
		inline constexpr auto             process_timeout             = std::chrono::seconds{ 30 };
		inline constexpr auto             pack_process_timeout        = std::chrono::minutes{ 5 };
		inline constexpr auto             reap_timeout                = std::chrono::seconds{ 2 };
		inline constexpr auto             start_timeout               = std::chrono::seconds{ 5 };
		inline constexpr std::size_t      standard_output_limit       = 1024U;
		inline constexpr std::size_t      standard_error_limit        = 16U * 1024U;
		inline constexpr std::size_t      input_chunk_bytes           = 16U * 1024U;
		inline constexpr std::uint64_t    maximum_pack_index_bytes    = 4ULL * 1024ULL * 1024ULL * 1024ULL;
		inline constexpr qint64           maximum_pending_input_bytes = 64 * 1024;
		inline constexpr std::string_view transaction_success         = "start: ok\nprepare: ok\ncommit: ok\n";

		struct process_success
		{
			int         ExitCode = 0;
			std::string StandardOutput;
		};

		using process_result = std::expected<process_success, git_process_error>;

		struct memory_input
		{
			std::string Bytes;
		};

		struct file_input
		{
			std::filesystem::path Path;
			std::uint64_t         Bytes = 0U;
		};

		using process_input = std::variant<memory_input, file_input>;

		struct process_request
		{
			std::vector<std::string>  Arguments;
			process_input             Input               = memory_input{ };
			std::chrono::milliseconds Timeout             = process_timeout;
			std::size_t               StandardOutputLimit = standard_output_limit;
			bool                      AllowExitOne        = false;
		};

		[[nodiscard]] auto path_text(
			const std::filesystem::path& aPath) -> std::string
		{
#if defined(Q_OS_WIN)
			const auto native = aPath.u8string();
			return {
				reinterpret_cast<const char*>(native.data()),
				native.size()
			};
#else
			return aPath.string();
#endif
		}

		// The exec-path reply's wire encoding is UTF-8, matching path_text's
		// re-encoding of the handshake expectation. Decoding the reply with the
		// narrow native constructor instead would make both probes disagree
		// about any non-ASCII path under a non-UTF-8 Windows code page, so no
		// consistent child reply could pass the handshake (BPR-0476). On POSIX
		// the u8 constructor preserves the reply bytes exactly.
		[[nodiscard]] auto path_from_text(
			const std::string_view aText) -> std::filesystem::path
		{
			return std::filesystem::path{
				std::u8string{
					reinterpret_cast<const char8_t*>(aText.data()),
					aText.size() }
			};
		}

		[[nodiscard]] auto decode_text(
			const std::string_view aValue,
			QString&               aDecoded) -> bool
		{
			if (
				aValue.size()
				> static_cast<std::size_t>(
					std::numeric_limits<qsizetype>::max())) {
				return false;
			}
			aDecoded = QString::fromUtf8(
				aValue.data(),
				static_cast<qsizetype>(aValue.size()));
			const auto encoded = aDecoded.toUtf8();
			return static_cast<std::size_t>(encoded.size()) == aValue.size()
				&& std::equal(
					   aValue.begin(),
					   aValue.end(),
					   encoded.constData());
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

		[[nodiscard]] auto resolve_canonical_existing(
			const std::filesystem::path& aPath)
			-> std::optional<std::filesystem::path>
		{
			std::error_code error;
			if (aPath.empty() || !aPath.is_absolute()) {
				return std::nullopt;
			}
			const auto canonical = std::filesystem::canonical(aPath, error);
			return error
				? std::nullopt
				: std::optional<std::filesystem::path>{ canonical };
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
				&& (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#else
	#if defined(Q_OS_WIN)
			const auto attributes = GetFileAttributesW(aPath.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES
				&& (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
	#else
			return true;
	#endif
#endif
		}

		[[nodiscard]] auto empty_private_directory(
			const std::filesystem::path& aPath) -> bool
		{
			if (!private_directory(aPath)) {
				return false;
			}
			std::error_code error;
			return std::filesystem::is_empty(aPath, error) && !error;
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
				&& (information.st_mode & (S_IRWXG | S_IRWXO)) == 0;
#else
	#if defined(Q_OS_WIN)
			// Attributes alone cannot see a hard link: the POSIX rule above
			// requires exactly one name (st_nlink == 1), and NTFS answers the
			// same question through the open handle (BPR-0437).
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
				&& (information.dwFileAttributes
					& FILE_ATTRIBUTE_REPARSE_POINT)
				== 0U
				&& (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				== 0U
				&& information.nNumberOfLinks == 1U;
	#else
			return true;
	#endif
#endif
		}

		class private_input_file final
		{
		public:
			private_input_file()                                             = default;
			private_input_file(const private_input_file&)                    = delete;
			auto operator=(const private_input_file&) -> private_input_file& = delete;

			~private_input_file()
			{
#if defined(Q_OS_UNIX)
				if (Descriptor >= 0) {
					static_cast<void>(::close(Descriptor));
				}
#elif defined(Q_OS_WIN)
				if (Handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CloseHandle(Handle));
				}
#endif
			}

			[[nodiscard]] auto open(
				const std::filesystem::path& aPath,
				const std::uint64_t          aExpectedBytes) -> bool
			{
				if (
					aPath.empty() || !aPath.is_absolute()
					|| !private_directory(aPath.parent_path())) {
					return false;
				}
#if defined(Q_OS_UNIX)
				Descriptor = ::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
				if (Descriptor < 0) {
					return false;
				}
				struct stat information{ };
				return ::fstat(Descriptor, &information) == 0
					&& S_ISREG(information.st_mode)
					&& information.st_uid == ::geteuid()
					&& information.st_nlink == 1
					&& (information.st_mode & (S_IRWXG | S_IRWXO)) == 0
					&& information.st_size >= 0
					&& static_cast<std::uint64_t>(information.st_size)
					== aExpectedBytes;
#elif defined(Q_OS_WIN)
				Handle = CreateFileW(
					aPath.c_str(),
					GENERIC_READ,
					0U,
					nullptr,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
						| FILE_FLAG_SEQUENTIAL_SCAN,
					nullptr);
				if (Handle == INVALID_HANDLE_VALUE) {
					return false;
				}
				FILE_ATTRIBUTE_TAG_INFO    tag{ };
				BY_HANDLE_FILE_INFORMATION information{ };
				LARGE_INTEGER              size{ };
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
					== aExpectedBytes;
#else
				static_cast<void>(aExpectedBytes);
				return false;
#endif
			}

			[[nodiscard]] auto read(const std::span<char> aDestination)
				-> std::optional<std::size_t>
			{
#if defined(Q_OS_UNIX)
				for (;;) {
					const auto received = ::read(
						Descriptor,
						aDestination.data(),
						aDestination.size());
					if (received < 0 && errno == EINTR) {
						continue;
					}
					if (received < 0) {
						return std::nullopt;
					}
					return static_cast<std::size_t>(received);
				}
#elif defined(Q_OS_WIN)
				const auto requested = static_cast<DWORD>(
					std::min<std::size_t>(
						aDestination.size(),
						std::numeric_limits<DWORD>::max()));
				DWORD received = 0U;
				if (
					ReadFile(
						Handle,
						aDestination.data(),
						requested,
						&received,
						nullptr)
					== 0) {
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
			const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::status(aPath, error);
			if (error || !std::filesystem::is_regular_file(status)) {
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

		[[nodiscard]] constexpr auto workspace_directory_identity(
			const exact_artifact_identity& aIdentity) noexcept
			-> application::agent_workspace_directory_identity
		{
			return {
				.Opaque = {
					static_cast<std::uint64_t>(aIdentity.Kind) + 1U,
					aIdentity.Volume,
					aIdentity.ObjectHigh,
					aIdentity.ObjectLow,
				},
			};
		}

		[[nodiscard]] constexpr auto exact_directory_identity(
			const application::agent_workspace_directory_identity& aIdentity) noexcept
			-> std::optional<exact_artifact_identity>
		{
			if (
				!application::valid(aIdentity)
				|| aIdentity.Opaque[0]
					!= static_cast<std::uint64_t>(exact_artifact_kind::directory) + 1U) {
				return std::nullopt;
			}
			return exact_artifact_identity{
				.Kind       = exact_artifact_kind::directory,
				.Volume     = aIdentity.Opaque[1],
				.ObjectHigh = aIdentity.Opaque[2],
				.ObjectLow  = aIdentity.Opaque[3],
			};
		}

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
			static_cast<void>(path);
			static_cast<void>(kind);
			return std::nullopt;
#endif
		}

		class process_boundary_lock final
		{
		public:
			explicit process_boundary_lock(
				const std::filesystem::path& aIsolation,
				const std::stop_token        aStop = { })
			{
				if (aStop.stop_requested()) {
					return;
				}
				const auto path     = aIsolation / ".git-process-boundary.lock";
				const auto deadline = clock::now() + start_timeout;
#if defined(Q_OS_UNIX)
				Descriptor = ::open(
					path.c_str(),
					O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_CREAT,
					S_IRUSR | S_IWUSR);
				if (Descriptor < 0) {
					return;
				}
				const auto identity = exact_identity(
					Descriptor,
					exact_artifact_kind::regular_file);
				if (
					!identity.has_value()
					|| ::fchmod(Descriptor, S_IRUSR | S_IWUSR) != 0) {
					return;
				}
				for (;;) {
					if (::flock(Descriptor, LOCK_EX | LOCK_NB) == 0) {
						Locked = true;
						return;
					}
					if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
						return;
					}
					if (clock::now() >= deadline) {
						return;
					}
					if (aStop.stop_requested()) {
						return;
					}
					std::this_thread::sleep_for(wait_slice);
				}
#elif defined(Q_OS_WIN)
				while (
					clock::now() < deadline
					&& !aStop.stop_requested()) {
					Handle = CreateFileW(
						path.c_str(),
						GENERIC_READ | GENERIC_WRITE,
						0U,
						nullptr,
						OPEN_ALWAYS,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
						nullptr);
					if (Handle != INVALID_HANDLE_VALUE) {
						const auto identity = exact_identity(
							Handle,
							exact_artifact_kind::regular_file);
						if (identity.has_value()) {
							Locked = true;
							return;
						}
						static_cast<void>(CloseHandle(Handle));
						Handle = INVALID_HANDLE_VALUE;
						return;
					}
					std::this_thread::sleep_for(wait_slice);
				}
#else
				static_cast<void>(path);
				static_cast<void>(deadline);
#endif
			}

			process_boundary_lock(const process_boundary_lock&) = delete;
			auto operator=(const process_boundary_lock&)
				-> process_boundary_lock& = delete;

			~process_boundary_lock()
			{
#if defined(Q_OS_UNIX)
				if (Descriptor >= 0) {
					if (Locked) {
						static_cast<void>(::flock(Descriptor, LOCK_UN));
					}
					static_cast<void>(::close(Descriptor));
				}
#elif defined(Q_OS_WIN)
				if (Handle != INVALID_HANDLE_VALUE) {
					static_cast<void>(CloseHandle(Handle));
				}
#endif
			}

			[[nodiscard]] auto valid() const noexcept -> bool
			{
				return Locked;
			}

		private:
			bool Locked = false;
#if defined(Q_OS_UNIX)
			int Descriptor = -1;
#elif defined(Q_OS_WIN)
			HANDLE Handle = INVALID_HANDLE_VALUE;
#endif
		};

		auto make_private_directory(
			const std::filesystem::path& aPath) -> bool
		{
			if (aPath.empty() || !aPath.is_absolute()) {
				return false;
			}
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (
				error
				&& error
					!= std::errc::no_such_file_or_directory) {
				return false;
			}
			if (
				!error
				&& status.type() != std::filesystem::file_type::not_found) {
				return private_directory(aPath);
			}
			error.clear();
			if (
				!private_directory(aPath.parent_path())
				|| !std::filesystem::create_directory(aPath, error)
				|| error) {
				return false;
			}
			std::filesystem::permissions(
				aPath,
				std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace,
				error);
			return !error && private_directory(aPath);
		}

		auto write_private_file(
			const std::filesystem::path&     aPath,
			const std::span<const std::byte> aBytes) -> bool
		{
			if (
				aPath.empty() || !aPath.is_absolute()
				|| !private_directory(aPath.parent_path())) {
				return false;
			}
#if defined(Q_OS_UNIX)
			struct stat before{ };
			const auto  existed = ::lstat(aPath.c_str(), &before) == 0;
			if (
				existed
				&& (!S_ISREG(before.st_mode)
					|| before.st_uid != ::geteuid()
					|| before.st_nlink != 1
					|| (before.st_mode & (S_IRWXG | S_IRWXO)) != 0)) {
				return false;
			}
			if (!existed && errno != ENOENT) {
				return false;
			}

			const auto flags = O_WRONLY | O_CLOEXEC | O_NOFOLLOW
				| (existed ? 0 : O_CREAT | O_EXCL);
			const auto descriptor = ::open(aPath.c_str(), flags, S_IRUSR | S_IWUSR);
			if (descriptor < 0) {
				return false;
			}
			const auto closeDescriptor =
				[descriptor]() noexcept {
					return ::close(descriptor) == 0;
				};

			struct stat opened{ };
			auto        valid = ::fstat(descriptor, &opened) == 0
				&& S_ISREG(opened.st_mode)
				&& opened.st_uid == ::geteuid()
				&& opened.st_nlink == 1
				&& (!existed
					|| (opened.st_dev == before.st_dev
						&& opened.st_ino == before.st_ino))
				&& ::fchmod(descriptor, S_IRUSR | S_IWUSR) == 0
				&& ::ftruncate(descriptor, 0) == 0;
			std::size_t offset = 0U;
			while (valid && offset < aBytes.size()) {
				const auto written = ::write(
					descriptor,
					aBytes.data() + offset,
					aBytes.size() - offset);
				if (written < 0 && errno == EINTR) {
					continue;
				}
				if (written <= 0) {
					valid = false;
					break;
				}
				offset += static_cast<std::size_t>(written);
			}
			valid             = valid && ::fsync(descriptor) == 0;
			const auto closed = closeDescriptor();
			return valid && closed && private_regular_file(aPath);
#elif defined(Q_OS_WIN)
			const auto attributes = GetFileAttributesW(aPath.c_str());
			const auto existed    = attributes != INVALID_FILE_ATTRIBUTES;
			if (
				existed
				&& ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
					|| (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)) {
				return false;
			}
			const auto handle = CreateFileW(
				aPath.c_str(),
				GENERIC_WRITE,
				0U,
				nullptr,
				existed ? OPEN_EXISTING : CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return false;
			}
			FILE_ATTRIBUTE_TAG_INFO    tag{ };
			BY_HANDLE_FILE_INFORMATION information{ };
			LARGE_INTEGER              beginning{ };
			auto                       valid = GetFileInformationByHandleEx(
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
			while (valid && offset < aBytes.size()) {
				const auto remaining = std::min<std::size_t>(
					aBytes.size() - offset,
					std::numeric_limits<DWORD>::max());
				DWORD written = 0U;
				valid         = WriteFile(
									handle,
									aBytes.data() + offset,
									static_cast<DWORD>(remaining),
									&written,
									nullptr)
						!= 0
					&& written != 0U;
				offset += written;
			}
			valid             = valid && FlushFileBuffers(handle) != 0;
			const auto closed = CloseHandle(handle) != 0;
			return valid && closed && private_regular_file(aPath);
#else
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(path, error);
			if (
				error
				|| (status.type() != std::filesystem::file_type::not_found
					&& (status.type()
							!= std::filesystem::file_type::regular
						|| std::filesystem::is_symlink(status)))) {
				return false;
			}
			try {
				std::ofstream output{
					path,
					std::ios::binary | std::ios::trunc
				};
				output.write(
					reinterpret_cast<const char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
				output.flush();
				output.close();
				if (!output) {
					return false;
				}
			} catch (...) {
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
			const std::filesystem::path& aPath,
			const std::string_view       aBytes) -> bool
		{
			return write_private_file(
				aPath,
				std::as_bytes(std::span{ aBytes }));
		}

		struct new_private_file_result
		{
			bool                                   Created  = false;
			bool                                   Complete = false;
			std::optional<exact_artifact_identity> Identity;
		};

		[[nodiscard]] auto write_new_private_file(
			const std::filesystem::path&     aPath,
			const std::span<const std::byte> aBytes)
			-> new_private_file_result
		{
			if (
				aPath.empty() || !aPath.is_absolute()
				|| !private_directory(aPath.parent_path())) {
				return { };
			}
#if defined(Q_OS_UNIX)
			const auto descriptor = ::open(
				aPath.c_str(),
				O_WRONLY | O_CLOEXEC | O_NOFOLLOW | O_CREAT | O_EXCL,
				S_IRUSR | S_IWUSR);
			if (descriptor < 0) {
				return { };
			}
			auto        complete = true;
			struct stat opened{ };
			complete = ::fstat(descriptor, &opened) == 0
				&& S_ISREG(opened.st_mode)
				&& opened.st_uid == ::geteuid()
				&& opened.st_nlink == 1
				&& (opened.st_mode & (S_IRWXG | S_IRWXO)) == 0;
			std::size_t offset = 0U;
			while (complete && offset < aBytes.size()) {
				const auto written = ::write(
					descriptor,
					aBytes.data() + offset,
					aBytes.size() - offset);
				if (written < 0 && errno == EINTR) {
					continue;
				}
				if (written <= 0) {
					complete = false;
					break;
				}
				offset += static_cast<std::size_t>(written);
			}
			complete          = complete && ::fsync(descriptor) == 0;
			const auto closed = ::close(descriptor) == 0;
			return {
				.Created  = true,
				.Complete = complete && closed && private_regular_file(aPath),
				.Identity = exact_identity(
					opened,
					exact_artifact_kind::regular_file),
			};
#elif defined(Q_OS_WIN)
			const auto handle = CreateFileW(
				aPath.c_str(),
				GENERIC_WRITE,
				0U,
				nullptr,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return { };
			}
			const auto identity = exact_identity(
				handle,
				exact_artifact_kind::regular_file);
			auto        complete = identity.has_value();
			std::size_t offset   = 0U;
			while (complete && offset < aBytes.size()) {
				const auto remaining = std::min<std::size_t>(
					aBytes.size() - offset,
					std::numeric_limits<DWORD>::max());
				DWORD written = 0U;
				complete      = WriteFile(
									handle,
									aBytes.data() + offset,
									static_cast<DWORD>(remaining),
									&written,
									nullptr)
						!= 0
					&& written != 0U;
				offset += written;
			}
			complete          = complete && FlushFileBuffers(handle) != 0;
			const auto closed = CloseHandle(handle) != 0;
			return {
				.Created  = true,
				.Complete = complete && closed && private_regular_file(aPath),
				.Identity = identity,
			};
#else
			std::ofstream output{
				path,
				std::ios::binary | std::ios::out | std::ios::noreplace
			};
			if (!output) {
				return { };
			}
			output.write(
				reinterpret_cast<const char*>(bytes.data()),
				static_cast<std::streamsize>(bytes.size()));
			output.flush();
			output.close();
			std::error_code error;
			std::filesystem::permissions(
				path,
				std::filesystem::perms::owner_read
					| std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace,
				error);
			return {
				.Created  = true,
				.Complete = !error && output && private_regular_file(path),
				.Identity = exact_identity(
					path,
					exact_artifact_kind::regular_file),
			};
#endif
		}

		[[nodiscard]] auto read_exact_file(
			const std::filesystem::path& aPath,
			const std::string_view       aExpected) -> bool
		{
			try {
				if (!private_regular_file(aPath)) {
					return false;
				}
				std::ifstream input{ aPath, std::ios::binary };
				if (!input) {
					return false;
				}
				std::string value{
					std::istreambuf_iterator<char>{ input },
					std::istreambuf_iterator<char>{ }
				};
				return !input.bad() && value == aExpected;
			} catch (...) {
				return false;
			}
		}

		[[nodiscard]] auto equal_private_files(
			const std::filesystem::path& aLeft,
			const std::filesystem::path& aRight,
			const std::uint64_t          aExpectedBytes) -> bool
		{
			try {
				if (
					!private_regular_file(aLeft)
					|| !private_regular_file(aRight)) {
					return false;
				}
				std::error_code error;
				const auto      leftSize = std::filesystem::file_size(aLeft, error);
				if (error || leftSize != aExpectedBytes) {
					return false;
				}
				const auto rightSize = std::filesystem::file_size(aRight, error);
				if (error || rightSize != aExpectedBytes) {
					return false;
				}

				std::ifstream leftInput{ aLeft, std::ios::binary };
				std::ifstream rightInput{ aRight, std::ios::binary };
				if (!leftInput || !rightInput) {
					return false;
				}
				std::array<char, 64U * 1024U> leftBuffer{ };
				std::array<char, 64U * 1024U> rightBuffer{ };
				auto                          remaining = aExpectedBytes;
				while (remaining != 0U) {
					const auto requested = static_cast<std::streamsize>(
						std::min<std::uint64_t>(
							leftBuffer.size(),
							remaining));
					leftInput.read(leftBuffer.data(), requested);
					rightInput.read(rightBuffer.data(), requested);
					if (
						leftInput.gcount() != requested
						|| rightInput.gcount() != requested
						|| !std::equal(
							leftBuffer.begin(),
							leftBuffer.begin() + requested,
							rightBuffer.begin())) {
						return false;
					}
					remaining -= static_cast<std::uint64_t>(requested);
				}
				return !leftInput.bad() && !rightInput.bad();
			} catch (...) {
				return false;
			}
		}

		[[nodiscard]] constexpr auto lower_hex_digit(
			const std::uint8_t aNibble) noexcept -> char
		{
			return aNibble < 10U
				? static_cast<char>('0' + aNibble)
				: static_cast<char>('a' + (aNibble - 10U));
		}

		template <typename Bytes>
		[[nodiscard]] auto lower_hex(const Bytes& aBytes) -> std::string
		{
			std::string result;
			result.reserve(aBytes.size() * 2U);
			for (const auto byte : aBytes) {
				result.push_back(lower_hex_digit(
					static_cast<std::uint8_t>(byte >> 4U)));
				result.push_back(lower_hex_digit(
					static_cast<std::uint8_t>(byte & 0x0fU)));
			}
			return result;
		}

		[[nodiscard]] constexpr auto hex_value(const char aValue) noexcept
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

		template <typename Object>
		[[nodiscard]] auto parse_object(
			const std::string_view   aText,
			const git_hash_algorithm aAlgorithm) -> std::optional<Object>
		{
			const auto byteCount = aAlgorithm == git_hash_algorithm::sha1 ? 20U : 32U;
			if (aText.size() != byteCount * 2U) {
				return std::nullopt;
			}
			Object result{ .Algorithm = aAlgorithm };
			for (std::size_t index = 0; index < byteCount; ++index) {
				const auto high = hex_value(aText[index * 2U]);
				const auto low  = hex_value(aText[index * 2U + 1U]);
				if (!high.has_value() || !low.has_value()) {
					return std::nullopt;
				}
				result.Digest[index] = static_cast<std::uint8_t>(
					static_cast<std::uint8_t>(*high << 4U) | *low);
			}
			if (!application::valid(result)) {
				return std::nullopt;
			}
			return result;
		}

		template <typename Object>
		[[nodiscard]] auto object_text(const Object& aObject) -> std::string
		{
			const auto byteCount = aObject.Algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
			return lower_hex(
				std::span{ aObject.Digest }.first(byteCount));
		}

		[[nodiscard]] auto valid_tree_entry_path(
			const std::filesystem::path& aPath) noexcept -> bool
		{
			const auto text = aPath.generic_u8string();
			return !text.empty() && !aPath.is_absolute()
				&& text.size() <= application::maximum_git_tree_entry_path_bytes
				&& text.front() != u8'/'
				&& text.find(u8'\0') == decltype(text)::npos
				&& std::ranges::none_of(
					aPath,
					[](const auto& aComponent) {
						return aComponent == "." || aComponent == "..";
					});
		}

		[[nodiscard]] auto tree_entry_path_text(
			const std::filesystem::path& aPath) -> std::string
		{
			const auto text = aPath.generic_u8string();
			return {
				reinterpret_cast<const char*>(text.data()),
				text.size()
			};
		}

		[[nodiscard]] auto tree_entry_path(
			const std::string_view aText) -> std::filesystem::path
		{
			const auto* const begin = reinterpret_cast<const char8_t*>(aText.data());
			return std::filesystem::path{
				std::u8string{ begin, begin + aText.size() }
			};
		}

		[[nodiscard]] auto validate_installed_pack(
			const std::filesystem::path& aRepository,
			const git_pack_id&           aPack,
			const std::filesystem::path& aSpool,
			const std::uint64_t          aExpectedBytes) -> bool
		{
			const auto packDirectory = aRepository / "objects" / "pack";
			if (!private_directory(packDirectory)) {
				return false;
			}
			const auto stem           = "pack-" + object_text(aPack);
			const auto installedPack  = packDirectory / (stem + ".pack");
			const auto installedIndex = packDirectory / (stem + ".idx");
			const auto reverseIndex   = packDirectory / (stem + ".rev");
			if (
				!equal_private_files(
					aSpool,
					installedPack,
					aExpectedBytes)
				|| !private_regular_file(installedIndex)) {
				return false;
			}
			std::error_code error;
			const auto      indexBytes = std::filesystem::file_size(installedIndex, error);
			if (
				error || indexBytes == 0U
				|| indexBytes > maximum_pack_index_bytes) {
				return false;
			}
			return !std::filesystem::exists(reverseIndex, error) && !error;
		}

		[[nodiscard]] auto valid_hash_algorithm(
			const git_hash_algorithm aAlgorithm) noexcept -> bool
		{
			return aAlgorithm == git_hash_algorithm::sha1
				|| aAlgorithm == git_hash_algorithm::sha256;
		}

		[[nodiscard]] auto valid_fixed_hex(
			const std::string_view aValue,
			const std::size_t      aSize) noexcept -> bool
		{
			return aValue.size() == aSize
				&& std::all_of(
					   aValue.begin(),
					   aValue.end(),
					   [](const char aCharacter) {
						   return hex_value(aCharacter).has_value();
					   });
		}

		[[nodiscard]] auto ordinal_text(
			const std::uint64_t aOrdinal) -> std::string
		{
			std::array<std::uint8_t, 8U> bytes{ };
			for (std::size_t index = 0; index < bytes.size(); ++index) {
				const auto shift = static_cast<unsigned int>((bytes.size() - index - 1U) * 8U);
				bytes[index]     = static_cast<std::uint8_t>(aOrdinal >> shift);
			}
			return lower_hex(bytes);
		}

		[[nodiscard]] auto ref_text(
			const application::git_private_ref& aRef)
			-> std::optional<std::string>
		{
			return std::visit(
				[](const auto& aValue) -> std::optional<std::string> {
					using value_type    = std::decay_t<decltype(aValue)>;
					const auto endpoint = aValue.Endpoint
							== application::git_private_ref_endpoint::base
						? "base"
						: aValue.Endpoint
							== application::git_private_ref_endpoint::head
						? "head"
						: nullptr;
					if (endpoint == nullptr) {
						return std::nullopt;
					}
					if constexpr (
						std::is_same_v<
							value_type,
							application::git_staging_ref>) {
						if (!application::valid_identity(aValue.Operation)) {
							return std::nullopt;
						}
						return "refs/betterpullreview/v1/stage/"
							+ lower_hex(aValue.Operation.Bytes) + "/" + endpoint;
					} else {
						if (
							!application::valid_identity(aValue.Scope)
							|| aValue.Ordinal == 0U
							|| aValue.Ordinal
								> static_cast<std::uint64_t>(
									std::numeric_limits<std::int64_t>::max())) {
							return std::nullopt;
						}
						return "refs/betterpullreview/v1/scope/"
							+ lower_hex(aValue.Scope.Bytes) + "/epoch/"
							+ ordinal_text(aValue.Ordinal) + "/" + endpoint;
					}
				},
				aRef);
		}

		[[nodiscard]] auto wait_milliseconds(
			const clock::time_point aDeadline) noexcept -> int
		{
			const auto remaining = aDeadline - clock::now();
			if (remaining <= clock::duration::zero()) {
				return 0;
			}
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
			if (milliseconds < std::chrono::milliseconds{ 1 }) {
				return 1;
			}
			return static_cast<int>(
				std::min(milliseconds, wait_slice).count());
		}

		auto kill_process_group(const qint64 aProcessGroupId) noexcept -> void
		{
#if defined(Q_OS_UNIX)
			if (
				aProcessGroupId > 1
				&& aProcessGroupId
					<= static_cast<qint64>(
						std::numeric_limits<pid_t>::max())) {
				static_cast<void>(::kill(
					-static_cast<pid_t>(aProcessGroupId),
					SIGKILL));
			}
#else
			static_cast<void>(aProcessGroupId);
#endif
		}

		[[nodiscard]] auto resolve_process_group(
			const QProcess& aProcess,
			const qint64    aKnownProcessGroupId) noexcept -> qint64
		{
			if (aKnownProcessGroupId > 1) {
				return aKnownProcessGroupId;
			}
			const auto processId = aProcess.processId();
			return processId > 1 ? processId : 0;
		}

		class windows_job final
		{
		public:
			windows_job()                                      = default;
			windows_job(const windows_job&)                    = delete;
			auto operator=(const windows_job&) -> windows_job& = delete;
			~windows_job()
			{
#if defined(Q_OS_WIN)
				if (Handle != nullptr) {
					static_cast<void>(CloseHandle(Handle));
				}
#endif
			}

			[[nodiscard]] auto attach(const qint64 aProcessId) noexcept -> bool
			{
#if defined(Q_OS_WIN)
				Handle = CreateJobObjectW(nullptr, nullptr);
				if (Handle == nullptr) {
					return false;
				}
				JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{ };
				limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				if (
					SetInformationJobObject(
						Handle,
						JobObjectExtendedLimitInformation,
						&limits,
						sizeof(limits))
					== 0) {
					return false;
				}
				const auto process = OpenProcess(
					PROCESS_SET_QUOTA | PROCESS_TERMINATE,
					FALSE,
					static_cast<DWORD>(aProcessId));
				if (process == nullptr) {
					return false;
				}
				const auto assigned = AssignProcessToJobObject(Handle, process) != 0;
				static_cast<void>(CloseHandle(process));
				return assigned;
#else
				static_cast<void>(aProcessId);
				return true;
#endif
			}

			auto terminate() noexcept -> void
			{
#if defined(Q_OS_WIN)
				if (Handle != nullptr) {
					static_cast<void>(TerminateJobObject(Handle, 1U));
				}
#endif
			}

		private:
#if defined(Q_OS_WIN)
			HANDLE Handle = nullptr;
#endif
		};

		enum class process_stream : std::uint8_t {
			standard_output,
			standard_error,
		};

		[[nodiscard]] auto drain_channel(
			QProcess&            aProcess,
			const process_stream aStream,
			const std::size_t    aLimit,
			std::size_t&         aConsumed,
			std::string*         aRetained)
			-> std::optional<git_process_failure>
		{
			const auto channel = aStream == process_stream::standard_output
				? QProcess::StandardOutput
				: QProcess::StandardError;
			const auto failure = aStream == process_stream::standard_output
				? git_process_failure::standard_output_limit
				: git_process_failure::standard_error_limit;
			aProcess.setReadChannel(channel);

			std::array<char, 16U * 1024U> buffer{ };
			while (aProcess.bytesAvailable() > 0) {
				const auto remaining = aLimit - aConsumed;
				const auto requested = std::min<std::size_t>(
					buffer.size(),
					remaining + 1U);
				const auto bytesRead = aProcess.read(
					buffer.data(),
					static_cast<qint64>(requested));
				if (bytesRead < 0) {
					return failure;
				}
				if (bytesRead == 0) {
					break;
				}
				const auto byteCount = static_cast<std::size_t>(bytesRead);
				if (byteCount > remaining) {
					return failure;
				}
				if (aRetained != nullptr) {
					aRetained->append(buffer.data(), byteCount);
				}
				aConsumed += byteCount;
			}
			return std::nullopt;
		}

		inline constexpr std::string_view diagnostic_whitespace    = " \t\n\v\f\r";
		inline constexpr std::string_view redacted_url_placeholder = "<redacted-url>";

		[[nodiscard]] auto remote_url_token(
			const std::string_view aToken) noexcept -> bool
		{
			if (aToken.find("://") != std::string_view::npos) {
				return true;
			}
			const auto separator          = aToken.find('@');
			const auto localPathSeparator = aToken.find_first_of("/\\");
			// SSH remotes have no path separator before `user@host:`. A separator
			// before the at-sign makes this a local path with a remote-shaped leaf.
			return separator != std::string_view::npos && separator != 0U
				&& separator + 1U < aToken.size()
				&& aToken[separator + 1U] != '{'
				&& (localPathSeparator == std::string_view::npos
					|| localPathSeparator > separator)
				&& aToken.find(':', separator + 1U) != std::string_view::npos;
		}

		/// \brief Removes remote tokens from retained subprocess diagnostic text.
		/// \details Replaces every whitespace-delimited URL or SSH-style remote
		/// token with a fixed placeholder, preserves all other text exactly, and
		/// bounds the result by the installed-Git standard-error limit.
		[[nodiscard]] auto redacted_diagnostic(
			const std::string_view aText) -> std::string
		{
			std::string result;
			result.reserve(std::min(aText.size(), standard_error_limit));
			std::size_t index = 0U;
			while (index < aText.size()) {
				const auto tokenStart = aText.find_first_not_of(
					diagnostic_whitespace,
					index);
				result.append(
					aText.substr(
						index,
						std::min(tokenStart, aText.size()) - index));
				if (tokenStart == std::string_view::npos) {
					break;
				}
				const auto tokenEnd = std::min(
					aText.find_first_of(diagnostic_whitespace, tokenStart),
					aText.size());
				const auto token = aText.substr(tokenStart, tokenEnd - tokenStart);
				result.append(
					remote_url_token(token)
						? redacted_url_placeholder
						: token);
				index = tokenEnd;
			}
			if (result.size() > standard_error_limit) {
				result.resize(standard_error_limit);
			}
			return result;
		}

		struct process_execution
		{
			process_result            Result;
			std::unique_ptr<QProcess> AwaitingReap;
			qint64                    ProcessGroupId = 0;
		};

		[[nodiscard]] auto kill_and_reap(
			QProcess&    aProcess,
			windows_job& aJob,
			qint64       aProcessGroupId) -> bool
		{
			const auto deadline = clock::now() + reap_timeout;
			aProcessGroupId     = resolve_process_group(aProcess, aProcessGroupId);
			aJob.terminate();
			kill_process_group(aProcessGroupId);
			if (aProcess.state() != QProcess::NotRunning) {
				aProcess.kill();
			}
			while (aProcess.state() != QProcess::NotRunning) {
				const auto wait = wait_milliseconds(deadline);
				if (wait == 0) {
					return false;
				}
				static_cast<void>(aProcess.waitForFinished(wait));
				aJob.terminate();
				aProcessGroupId = resolve_process_group(aProcess, aProcessGroupId);
				kill_process_group(aProcessGroupId);
				if (aProcess.state() != QProcess::NotRunning) {
					aProcess.kill();
				}
			}
			kill_process_group(aProcessGroupId);
			return true;
		}

		[[nodiscard]] auto finish_background_reap(
			QProcess&    aProcess,
			const qint64 aKnownProcessGroupId) -> bool
		{
			const auto deadline       = clock::now() + reap_timeout;
			auto       processGroupId = resolve_process_group(aProcess, aKnownProcessGroupId);
			while (aProcess.state() != QProcess::NotRunning) {
				const auto wait = wait_milliseconds(deadline);
				if (wait == 0) {
					return false;
				}
				kill_process_group(processGroupId);
				aProcess.kill();
				static_cast<void>(aProcess.waitForFinished(
					wait));
				processGroupId = resolve_process_group(aProcess, processGroupId);
			}
			kill_process_group(processGroupId);
			return true;
		}

		auto abandon_unreaped_process(
			std::unique_ptr<QProcess>& aProcess) noexcept -> void
		{
			static_cast<void>(
				QObject::disconnect(
					aProcess.get(),
					nullptr,
					aProcess.get(),
					nullptr));
			// Destruction of a still-running QProcess may wait. The worker is already
			// permanently quarantined, so retain this one catastrophic wrapper until
			// operating-system process cleanup rather than make shutdown unbounded.
			static_cast<void>(aProcess.release());
		}

		struct runner_configuration
		{
			std::filesystem::path                Executable;
			std::filesystem::path                WorkingDirectory;
			std::filesystem::path                Home;
			std::filesystem::path                Xdg;
			std::filesystem::path                Temporary;
			std::filesystem::path                SystemConfig;
			std::filesystem::path                GlobalConfig;
			std::filesystem::path                Attributes;
			std::filesystem::path                Templates;
			std::filesystem::path                Hooks;
			std::optional<std::filesystem::path> ExecPath;
			std::optional<std::filesystem::path> WindowsSystemRoot;
		};

		[[nodiscard]] auto process_environment(
			const runner_configuration& aConfiguration)
			-> QProcessEnvironment
		{
			QProcessEnvironment environment;
			const auto          insert =
				[&environment](
					const std::string_view       aName,
					const std::filesystem::path& aValue) {
					QString decodedName;
					QString decodedValue;
					if (
						!decode_text(aName, decodedName)
						|| !decode_text(path_text(aValue), decodedValue)) {
						return false;
					}
					environment.insert(decodedName, decodedValue);
					return true;
				};
			const auto insertText =
				[&environment](
					const std::string_view aName,
					const std::string_view aValue) {
					QString decodedName;
					QString decodedValue;
					if (
						!decode_text(aName, decodedName)
						|| !decode_text(aValue, decodedValue)) {
						return false;
					}
					environment.insert(decodedName, decodedValue);
					return true;
				};

			if (
				!insert("HOME", aConfiguration.Home)
				|| !insert("XDG_CONFIG_HOME", aConfiguration.Xdg)
				|| !insert("TMPDIR", aConfiguration.Temporary)
				|| !insert("TMP", aConfiguration.Temporary)
				|| !insert("TEMP", aConfiguration.Temporary)
				|| !insert("GIT_CONFIG_SYSTEM", aConfiguration.SystemConfig)
				|| !insert("GIT_CONFIG_GLOBAL", aConfiguration.GlobalConfig)
				|| !insert("GIT_ATTR_SYSTEM", aConfiguration.Attributes)
				|| !insert("GIT_TEMPLATE_DIR", aConfiguration.Templates)
				|| !insertText("GIT_CONFIG_NOSYSTEM", "1")
				|| !insertText("GIT_ATTR_NOSYSTEM", "1")
				|| !insertText("GIT_TERMINAL_PROMPT", "0")
				|| !insertText("GIT_NO_REPLACE_OBJECTS", "1")
				|| !insertText("GIT_NO_LAZY_FETCH", "1")
				|| !insertText("GIT_OPTIONAL_LOCKS", "0")
				|| !insertText("GIT_FLUSH", "1")
				|| !insertText("LC_ALL", "C")
				|| !insertText("LANG", "C")) {
				return { };
			}

			const auto executableParent = aConfiguration.Executable.parent_path();
			auto       searchPath       = executableParent;
			if (aConfiguration.ExecPath.has_value()) {
				if (!insert("GIT_EXEC_PATH", *aConfiguration.ExecPath)) {
					return { };
				}
				searchPath = *aConfiguration.ExecPath;
				if (executableParent != searchPath) {
					searchPath +=
#if defined(Q_OS_WIN)
						";";
#else
						":";
#endif
					searchPath += executableParent;
				}
			}
			if (!insert("PATH", searchPath)) {
				return { };
			}
#if defined(Q_OS_WIN)
			if (
				!aConfiguration.WindowsSystemRoot.has_value()
				|| !insert(
					"SystemRoot",
					*aConfiguration.WindowsSystemRoot)) {
				return { };
			}
#endif
			return environment;
		}

		[[nodiscard]] auto decode_arguments(
			const process_request& aRequest,
			QStringList&           aArguments) -> bool
		{
			if (aRequest.Arguments.empty() || aRequest.Arguments.size() > 64U) {
				return false;
			}
			for (const auto& argument : aRequest.Arguments) {
				if (
					argument.empty() || argument.size() > 4096U
					|| argument.find('\0') != std::string::npos) {
					return false;
				}
				QString decoded;
				if (!decode_text(argument, decoded)) {
					return false;
				}
				aArguments.push_back(std::move(decoded));
			}
			return true;
		}

		[[nodiscard]] auto failed_execution(
			std::unique_ptr<QProcess>& aProcess,
			windows_job&               aJob,
			const qint64               aProcessGroupId,
			git_process_error          aFailure) -> process_execution
		{
			if (kill_and_reap(*aProcess, aJob, aProcessGroupId)) {
				return {
					.Result         = std::unexpected{ std::move(aFailure) },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			return {
				.Result         = std::unexpected{ git_process_failure::reap_failure },
				.AwaitingReap   = std::move(aProcess),
				.ProcessGroupId = aProcessGroupId,
			};
		}

		[[nodiscard]] auto execute_process(
			const runner_configuration& aConfiguration,
			const process_request&      aRequest,
			const std::stop_token       aStop,
			const std::atomic_bool&     aShutdownRequested,
			const clock::time_point     aDeadline) -> process_execution
		{
			const auto canonicalExecutable = canonical_existing(aConfiguration.Executable);
			if (
				!canonicalExecutable.has_value()
				|| *canonicalExecutable != aConfiguration.Executable
				|| !regular_executable(*canonicalExecutable)
				|| !canonical_existing(
						aConfiguration.WorkingDirectory)
						.has_value()
				|| !empty_private_directory(aConfiguration.Templates)
				|| !empty_private_directory(aConfiguration.Hooks)
				|| (aConfiguration.ExecPath.has_value()
					&& canonical_existing(*aConfiguration.ExecPath)
						!= aConfiguration.ExecPath)) {
				return {
					.Result = std::unexpected{
						git_process_failure::invalid_configuration },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			QString     executable;
			QString     workingDirectory;
			QStringList arguments;
			const auto  environment = process_environment(aConfiguration);
			if (
				!decode_text(path_text(aConfiguration.Executable), executable)
				|| !decode_text(
					path_text(aConfiguration.WorkingDirectory),
					workingDirectory)
				|| !decode_arguments(aRequest, arguments)
				|| environment.isEmpty()) {
				return {
					.Result = std::unexpected{
						git_process_failure::invalid_configuration },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			if (
				aStop.stop_requested()
				|| aShutdownRequested.load(std::memory_order_acquire)) {
				return {
					.Result         = std::unexpected{ git_process_failure::cancelled },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}

			qint64      processGroupId = 0;
			auto        process        = std::make_unique<QProcess>();
			windows_job job;
			try {
				process->setProgram(executable);
				process->setArguments(arguments);
				process->setProcessEnvironment(environment);
				process->setWorkingDirectory(workingDirectory);
				process->setProcessChannelMode(QProcess::SeparateChannels);
				process->setInputChannelMode(QProcess::ManagedInputChannel);
#if defined(Q_OS_UNIX)
				process->setUnixProcessParameters(
					QProcess::UnixProcessFlag::CloseFileDescriptors
					| QProcess::UnixProcessFlag::DisableCoreDumps
					| QProcess::UnixProcessFlag::ResetSignalHandlers
					| QProcess::UnixProcessFlag::CreateNewSession);
				process->setChildProcessModifier(
					[] {
						static_cast<void>(::umask(S_IRWXG | S_IRWXO));
					});
#endif
				QObject::connect(
					process.get(),
					&QProcess::started,
					process.get(),
					[processPointer = process.get(),
					 &processGroupId]() noexcept {
						processGroupId = processPointer->processId();
					},
					Qt::DirectConnection);
				QObject::connect(
					process.get(),
					&QProcess::finished,
					process.get(),
					[&processGroupId](
						const int,
						const QProcess::ExitStatus) noexcept {
						kill_process_group(processGroupId);
					},
					Qt::DirectConnection);

				std::string standardOutput;
				standardOutput.reserve(aRequest.StandardOutputLimit);
				std::string standardError;
				std::size_t standardOutputBytes = 0U;
				std::size_t standardErrorBytes  = 0U;
				const auto  drainStreams =
					[&]() -> std::optional<git_process_failure> {
					if (
						const auto failure = drain_channel(
							*process,
							process_stream::standard_output,
							aRequest.StandardOutputLimit,
							standardOutputBytes,
							&standardOutput);
						failure.has_value()) {
						return failure;
					}
					if (
						const auto failure = drain_channel(
							*process,
							process_stream::standard_error,
							standard_error_limit,
							standardErrorBytes,
							&standardError);
						failure.has_value()) {
						return failure;
					}
					process->setReadChannel(QProcess::StandardOutput);
					return std::nullopt;
				};
				const auto terminalFailure =
					[&]() -> std::optional<git_process_failure> {
					if (
						aStop.stop_requested()
						|| aShutdownRequested.load(std::memory_order_acquire)) {
						return git_process_failure::cancelled;
					}
					if (clock::now() >= aDeadline) {
						return git_process_failure::timeout;
					}
					return std::nullopt;
				};
				const auto failed =
					[&](git_process_error aFailure)
					-> process_execution {
					std::fill(
						standardOutput.begin(),
						standardOutput.end(),
						'\0');
					standardOutput.clear();
					std::fill(
						standardError.begin(),
						standardError.end(),
						'\0');
					standardError.clear();
					return failed_execution(
						process,
						job,
						processGroupId,
						std::move(aFailure));
				};

				const auto startDeadline = std::min(aDeadline, clock::now() + start_timeout);
				process->start();
				for (;;) {
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}
					if (clock::now() >= startDeadline) {
						return failed(git_process_failure::process_start);
					}
					if (process->waitForStarted(
							wait_milliseconds(startDeadline))) {
						break;
					}
					if (process->state() == QProcess::NotRunning) {
						if (
							process->error()
							== QProcess::FailedToStart) {
							return failed(
								git_process_failure::process_start);
						}
						break;
					}
				}
				processGroupId = resolve_process_group(*process, 0);
				if (
					process->state() != QProcess::NotRunning
					&& !job.attach(process->processId())) {
					return failed(git_process_failure::process_start);
				}

				private_input_file inputFile;
				const char*        memoryData = nullptr;
				std::uint64_t      inputSize  = 0U;
				if (const auto* memory = std::get_if<memory_input>(&aRequest.Input)) {
					memoryData = memory->Bytes.data();
					inputSize  = memory->Bytes.size();
				} else {
					const auto& file = std::get<file_input>(aRequest.Input);
					if (
						!inputFile.open(file.Path, file.Bytes)) {
						return failed(git_process_failure::standard_input);
					}
					inputSize = file.Bytes;
				}

				std::array<char, input_chunk_bytes> inputBuffer{ };
				std::uint64_t                       inputOffset         = 0U;
				std::size_t                         bufferedInputOffset = 0U;
				std::size_t                         bufferedInputSize   = 0U;
				bool                                inputClosed         = false;
				while (process->state() != QProcess::NotRunning) {
					if (
						inputOffset < inputSize
						&& process->bytesToWrite()
							<= maximum_pending_input_bytes) {
						const auto remaining = inputSize - inputOffset;
						auto       requested = std::min<std::uint64_t>(
							input_chunk_bytes,
							remaining);
						const char* bytes = nullptr;
						if (memoryData != nullptr) {
							bytes = memoryData + inputOffset;
						} else {
							if (bufferedInputOffset == bufferedInputSize) {
								const auto received = inputFile.read(
									std::span{ inputBuffer }.first(
										static_cast<std::size_t>(requested)));
								if (
									!received.has_value()
									|| *received == 0U) {
									return failed(
										git_process_failure::standard_input);
								}
								bufferedInputOffset = 0U;
								bufferedInputSize   = *received;
							}
							requested = bufferedInputSize - bufferedInputOffset;
							bytes     = inputBuffer.data() + bufferedInputOffset;
						}
						const auto accepted = process->write(
							bytes,
							static_cast<qint64>(requested));
						if (accepted < 0) {
							return failed(
								git_process_failure::standard_input);
						}
						inputOffset += static_cast<std::uint64_t>(accepted);
						if (memoryData == nullptr) {
							bufferedInputOffset += static_cast<std::size_t>(accepted);
						}
					}
					if (
						inputOffset == inputSize && !inputClosed) {
						process->closeWriteChannel();
						inputClosed = true;
					}
					if (const auto failure = drainStreams(); failure.has_value()) {
						return failed(*failure);
					}
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}
					process->setReadChannel(QProcess::StandardOutput);
					static_cast<void>(process->waitForReadyRead(
						wait_milliseconds(aDeadline)));
					if (process->bytesToWrite() > 0) {
						static_cast<void>(
							process->waitForBytesWritten(0));
					}
				}
				if (const auto failure = drainStreams(); failure.has_value()) {
					return failed(*failure);
				}
				kill_process_group(processGroupId);
				if (inputOffset != inputSize) {
					return failed(git_process_failure::standard_input);
				}
				if (process->exitStatus() == QProcess::CrashExit) {
					return failed(git_process_failure::process_crashed);
				}
				const auto exitCode = process->exitCode();
				if (
					exitCode != 0
					&& !(aRequest.AllowExitOne && exitCode == 1)) {
					return failed(
						git_process_error{
							git_process_failure::nonzero_exit,
							redacted_diagnostic(standardError) });
				}
				// An accepted exit that wrote to standard error is malformed output,
				// not an explanation, so it carries no retained text.
				if (standardErrorBytes != 0U) {
					return failed(git_process_failure::malformed_output);
				}
				return {
					.Result = process_success{
						.ExitCode       = exitCode,
						.StandardOutput = std::move(standardOutput),
					},
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			} catch (...) {
				return failed_execution(
					process,
					job,
					processGroupId,
					git_process_failure::process_start);
			}
		}

		struct process_work_item
		{
			process_request               Request;
			std::stop_token               Stop;
			clock::time_point             Deadline;
			std::mutex                    Mutex;
			std::condition_variable       Completed;
			std::optional<process_result> Result;
		};

		class process_worker final
		{
		public:
			explicit process_worker(runner_configuration aConfiguration)
				: Configuration{ std::move(aConfiguration) }
				, Worker{ [this] {
					worker_loop();
				} }
			{
			}

			process_worker(const process_worker&)                    = delete;
			auto operator=(const process_worker&) -> process_worker& = delete;

			~process_worker()
			{
				ShutdownRequested.store(true, std::memory_order_release);
				WorkAvailable.notify_all();
				SlotAvailable.notify_all();
				if (Worker.joinable()) {
					Worker.join();
				}
			}

			[[nodiscard]] auto submit(
				process_request       aRequest,
				const std::stop_token aStop) -> process_result
			{
				if (aStop.stop_requested()) {
					return std::unexpected{ git_process_failure::cancelled };
				}
				std::shared_ptr<process_work_item> item;
				try {
					item           = std::make_shared<process_work_item>();
					item->Request  = std::move(aRequest);
					item->Stop     = aStop;
					item->Deadline = clock::now() + item->Request.Timeout;
				} catch (...) {
					return std::unexpected{
						git_process_failure::process_start
					};
				}

				{
					std::unique_lock lock{ Mutex };
					while (Busy) {
						if (
							ShutdownRequested.load(std::memory_order_acquire)
							|| aStop.stop_requested()) {
							return std::unexpected{
								git_process_failure::cancelled
							};
						}
						if (Quarantined) {
							return std::unexpected{
								git_process_failure::reap_failure
							};
						}
						if (clock::now() >= item->Deadline) {
							return std::unexpected{
								git_process_failure::timeout
							};
						}
						SlotAvailable.wait_for(lock, wait_slice);
					}
					if (Quarantined) {
						return std::unexpected{
							git_process_failure::reap_failure
						};
					}
					if (ShutdownRequested.load(std::memory_order_acquire)) {
						return std::unexpected{
							git_process_failure::cancelled
						};
					}
					Busy    = true;
					Pending = item;
				}
				WorkAvailable.notify_one();

				std::unique_lock resultLock{ item->Mutex };
				const auto       completionDeadline = item->Deadline + reap_timeout + wait_slice;
				if (
					!item->Completed.wait_until(
						resultLock,
						completionDeadline,
						[&] {
							return item->Result.has_value();
						})) {
					{
						std::lock_guard lock{ Mutex };
						Quarantined = true;
					}
					SlotAvailable.notify_all();
					return std::unexpected{
						git_process_failure::reap_failure
					};
				}
				return std::move(*item->Result);
			}

		private:
			auto publish(
				const std::shared_ptr<process_work_item>& aItem,
				process_result                            aResult) -> void
			{
				{
					std::lock_guard lock{ aItem->Mutex };
					aItem->Result.emplace(std::move(aResult));
				}
				aItem->Completed.notify_one();
			}

			auto worker_loop() -> void
			{
				for (;;) {
					std::shared_ptr<process_work_item> item;
					{
						std::unique_lock lock{ Mutex };
						WorkAvailable.wait(
							lock,
							[&] {
								return Pending != nullptr
									|| ShutdownRequested.load(
										std::memory_order_acquire);
							});
						if (Pending == nullptr) {
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
					if (execution.AwaitingReap != nullptr) {
						{
							std::lock_guard lock{ Mutex };
							Quarantined = true;
						}
						SlotAvailable.notify_all();
					}
					publish(item, std::move(execution.Result));
					bool reaped = true;
					if (execution.AwaitingReap != nullptr) {
						reaped = finish_background_reap(
							*execution.AwaitingReap,
							execution.ProcessGroupId);
						if (reaped) {
							execution.AwaitingReap.reset();
						} else {
							abandon_unreaped_process(
								execution.AwaitingReap);
						}
					}
					{
						std::lock_guard lock{ Mutex };
						Busy        = false;
						Quarantined = !reaped;
					}
					SlotAvailable.notify_all();
				}
			}

			runner_configuration               Configuration;
			std::mutex                         Mutex;
			std::condition_variable            WorkAvailable;
			std::condition_variable            SlotAvailable;
			std::shared_ptr<process_work_item> Pending;
			bool                               Busy              = false;
			bool                               Quarantined       = false;
			std::atomic_bool                   ShutdownRequested = false;
			std::thread                        Worker;
		};

		[[nodiscard]] auto repository_config(
			const git_hash_algorithm aAlgorithm) -> std::string
		{
			auto result = std::string{
				"[core]\n"
				"\trepositoryformatversion = 0\n"
				"\tbare = true\n"
				"\tlogallrefupdates = false\n"
			};
			if (aAlgorithm == git_hash_algorithm::sha256) {
				result = "[core]\n"
						 "\trepositoryformatversion = 1\n"
						 "\tbare = true\n"
						 "\tlogallrefupdates = false\n"
						 "[extensions]\n"
						 "\tobjectformat = sha256\n";
			}
			return result;
		}

		[[nodiscard]] auto common_git_arguments(
			const std::filesystem::path& aRepository,
			const std::filesystem::path& aHooks) -> std::vector<std::string>
		{
			return {
				"-c",
				"core.fsync=all",
				"-c",
				"core.fsyncMethod=fsync",
				"-c",
				"core.hooksPath=" + path_text(aHooks),
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
				"--git-dir=" + path_text(aRepository),
			};
		}

		[[nodiscard]] auto workspace_git_arguments(
			const std::filesystem::path& aWorkspace,
			const std::filesystem::path& aHooks) -> std::vector<std::string>
		{
			return {
				"-c",
				"core.fsync=all",
				"-c",
				"core.fsyncMethod=fsync",
				"-c",
				"core.hooksPath=" + path_text(aHooks),
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
				"-C",
				path_text(aWorkspace),
			};
		}

		[[nodiscard]] auto path_begins_with(
			const std::filesystem::path& aPath,
			const std::filesystem::path& aPrefix) noexcept -> bool
		{
			auto path   = aPath.begin();
			auto prefix = aPrefix.begin();
			for (; prefix != aPrefix.end(); ++prefix, ++path) {
				if (path == aPath.end() || *path != *prefix) {
					return false;
				}
			}
			return true;
		}

		template <typename... Arguments>
		auto append_arguments(
			std::vector<std::string>& aTarget,
			Arguments&&... aArguments) -> void
		{
			(aTarget.emplace_back(std::forward<Arguments>(aArguments)), ...);
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
#elif defined(Q_OS_LINUX) && defined(SYS_renameat2)
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
#elif defined(Q_OS_LINUX)
			static_cast<void>(aSource);
			static_cast<void>(aDestination);
			aError = std::make_error_code(
				std::errc::operation_not_supported);
			return false;
#elif defined(Q_OS_WIN)
			if (
				MoveFileExW(
					aSource.c_str(),
					aDestination.c_str(),
					MOVEFILE_WRITE_THROUGH)
				!= 0) {
				return true;
			}
			aError = std::error_code{
				static_cast<int>(GetLastError()),
				std::system_category()
			};
			return false;
#else
			static_cast<void>(source);
			static_cast<void>(destination);
			error = std::make_error_code(
				std::errc::operation_not_supported);
			return false;
#endif
		}

		[[nodiscard]] auto quarantine_and_remove_exact(
			std::filesystem::path&         aPath,
			const exact_artifact_identity& aExpected,
			const bool                     aRecursive) noexcept -> bool
		{
			try {
				std::random_device    random;
				constexpr std::size_t maximumQuarantineAttempts = 8U;
				for (
					std::size_t attempt = 0U;
					attempt < maximumQuarantineAttempts;
					++attempt) {
					if (exact_identity(aPath, aExpected.Kind) != aExpected) {
						return false;
					}
					std::array<std::uint8_t, 16U> nonce{ };
					for (auto& byte : nonce) {
						byte = static_cast<std::uint8_t>(random());
					}
					auto quarantine = aPath.parent_path()
						/ (".bpr-cleanup-" + lower_hex(nonce));
#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
					const auto      isolation = aPath.parent_path().parent_path().parent_path();
					std::error_code injectionMarkerError;
					const auto      injectDestination = attempt == 0U
						&& std::filesystem::exists(
													   isolation
														   / "inject-probe-quarantine-destination",
													   injectionMarkerError)
						&& !injectionMarkerError;
					if (injectDestination) {
						quarantine = aPath.parent_path()
							/ (aRecursive
								   ? ".bpr-cleanup-destination-repository"
								   : ".bpr-cleanup-destination-spool");
					}
#endif
					std::error_code presenceError;
					if (
						std::filesystem::exists(
							quarantine,
							presenceError)
						|| presenceError) {
						continue;
					}
#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
					if (injectDestination) {
						const auto injected = aRecursive
							? make_private_directory(quarantine)
								&& write_private_file(
									quarantine / "destination-canary",
									"quarantine-repository-canary")
							: write_private_file(
								  quarantine,
								  "quarantine-spool-canary");
						if (!injected) {
							return false;
						}
					}
#endif
					std::error_code renameError;
					if (!rename_no_replace(aPath, quarantine, renameError)) {
						std::error_code destinationError;
						if (
							std::filesystem::exists(
								quarantine,
								destinationError)
							&& !destinationError) {
							continue;
						}
						return false;
					}
					const auto quarantined = exact_identity(quarantine, aExpected.Kind);
					if (quarantined != aExpected) {
						std::error_code originalError;
						const auto      originalPresent = std::filesystem::exists(aPath, originalError);
						if (!originalError && !originalPresent) {
							std::error_code restoreError;
							static_cast<void>(rename_no_replace(
								quarantine,
								aPath,
								restoreError));
						}
						return false;
					}
					aPath = quarantine;

					std::error_code removeError;
					if (aRecursive) {
						static_cast<void>(
							std::filesystem::remove_all(aPath, removeError));
					} else {
						static_cast<void>(
							std::filesystem::remove(aPath, removeError));
					}
					std::error_code removalPresenceError;
					const auto      present = std::filesystem::exists(
						aPath,
						removalPresenceError);
					return !removeError && !removalPresenceError && !present;
				}
				return false;
			} catch (...) {
				return false;
			}
		}

		class installed_git_process final
			: public application::git_process_port
		{
		public:
			installed_git_process(
				application::git_installation   aInstallation,
				std::filesystem::path           aIsolationRoot,
				std::unique_ptr<process_worker> aWorker)
				: Installation{ std::move(aInstallation) }
				, IsolationRoot{ std::move(aIsolationRoot) }
				, RepositoriesRoot{ IsolationRoot / "repositories" / "v1" }
				, IncomingRoot{ IsolationRoot / "incoming" / "v1" }
				, TemplatesRoot{ IsolationRoot / "templates" }
				, HooksRoot{ IsolationRoot / "hooks" }
				, Worker{ std::move(aWorker) }
			{
			}

			[[nodiscard]] auto installation() const noexcept
				-> const application::git_installation& override
			{
				return Installation;
			}

			[[nodiscard]] auto pack_spool_path(
				const application::git_operation_identity& aOperation) const
				-> git_process_result<std::filesystem::path> override
			{
				if (
					!application::valid_identity(aOperation)
					|| !private_directory(IncomingRoot)) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				return IncomingRoot
					/ (lower_hex(aOperation.Bytes) + ".pack.part");
			}

			[[nodiscard]] auto initialize_bare_repository(
				const application::git_initialize_repository_request& aRequest,
				const std::stop_token                                 aStop)
				-> git_process_result<void> override
			{
				auto boundary = process_boundary_lock{ IsolationRoot, aStop };
				if (!boundary.valid()) {
					return std::unexpected{
						aStop.stop_requested()
							? git_process_failure::cancelled
							: git_process_failure::invalid_configuration
					};
				}
				const auto initialized = initialize_owned_bare_repository(aRequest, aStop);
				if (!initialized.has_value()) {
					return std::unexpected{ initialized.error() };
				}
				return { };
			}

			[[nodiscard]] auto index_pack(
				const application::git_index_pack_request& aRequest,
				const std::stop_token                      aStop)
				-> git_process_result<
					application::git_index_pack_success> override
			{
				if (
					!application::valid_identity(aRequest.Operation)
					|| !application::valid(aRequest.ExpectedPack)
					|| aRequest.SpoolBytes == 0U
					|| aRequest.MaximumInputBytes < aRequest.SpoolBytes
					|| aRequest.MaximumInputBytes
						> application::maximum_git_pack_input_bytes) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (
					!valid_repository(
						aRequest.Repository,
						aRequest.ExpectedPack.Algorithm)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				const auto spool = pack_spool_path(aRequest.Operation);
				if (!spool.has_value()) {
					return std::unexpected{ spool.error() };
				}
				std::error_code error;
				if (
					!private_regular_file(*spool)
					|| std::filesystem::file_size(*spool, error)
						!= aRequest.SpoolBytes
					|| error) {
					return std::unexpected{
						git_process_failure::standard_input
					};
				}

				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"index-pack",
					"--stdin",
					"--strict",
					"--check-self-contained-and-connected",
					"--max-input-size="
						+ std::to_string(aRequest.MaximumInputBytes),
					"--threads=1",
					"--no-rev-index");
				const auto result = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = file_input{
							.Path  = *spool,
							.Bytes = aRequest.SpoolBytes,
						},
						.Timeout = pack_process_timeout,
					},
					aStop);
				if (!result.has_value()) {
					return std::unexpected{ result.error() };
				}
				const auto expected = "pack\t" + object_text(aRequest.ExpectedPack) + "\n";
				if (result->StandardOutput != expected) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				if (
					!validate_installed_pack(
						aRequest.Repository,
						aRequest.ExpectedPack,
						*spool,
						aRequest.SpoolBytes)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				return application::git_index_pack_success{
					.Pack = aRequest.ExpectedPack
				};
			}

			[[nodiscard]] auto inspect_commit(
				const application::git_commit_request& aRequest,
				const std::stop_token                  aStop)
				-> git_process_result<
					application::git_commit_inspection> override
			{
				if (
					!application::valid(aRequest.Commit)) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (
					!valid_repository(
						aRequest.Repository,
						aRequest.Commit.Algorithm)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				const auto commit    = object_text(aRequest.Commit);
				auto       arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"cat-file",
					"--batch-check=%(objectname) %(objecttype)");
				const auto exists = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = memory_input{ commit + "\n" },
					},
					aStop);
				if (!exists.has_value()) {
					return std::unexpected{ exists.error() };
				}
				if (exists->StandardOutput == commit + " missing\n") {
					return std::unexpected{
						git_process_failure::object_unavailable
					};
				}
				if (exists->StandardOutput != commit + " commit\n") {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}

				arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"rev-parse",
					"--verify",
					"--end-of-options",
					commit + "^{tree}");
				const auto tree = submit(
					process_request{ .Arguments = std::move(arguments) },
					aStop);
				if (!tree.has_value()) {
					return std::unexpected{ tree.error() };
				}
				if (
					tree->StandardOutput.empty()
					|| tree->StandardOutput.back() != '\n') {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				const auto parsed = parse_object<git_tree_id>(
					std::string_view{ tree->StandardOutput }.substr(
						0U,
						tree->StandardOutput.size() - 1U),
					aRequest.Commit.Algorithm);
				if (!parsed.has_value()) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"cat-file",
					"-e",
					object_text(*parsed) + "^{tree}");
				const auto treeExists = submit(
					process_request{ .Arguments = std::move(arguments) },
					aStop);
				if (!treeExists.has_value()) {
					return std::unexpected{ treeExists.error() };
				}
				if (!treeExists->StandardOutput.empty()) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				return application::git_commit_inspection{
					.Tree = *parsed
				};
			}

			[[nodiscard]] auto materialize_agent_workspace(
				const application::git_agent_workspace_request& aRequest,
				const std::stop_token                           aStop)
				-> git_process_result<application::owned_agent_workspace> override
			{
				auto boundary = process_boundary_lock{ IsolationRoot, aStop };
				if (!boundary.valid()) {
					return std::unexpected{
						aStop.stop_requested()
							? git_process_failure::cancelled
							: git_process_failure::invalid_configuration
					};
				}
				const auto& materialization = aRequest.Materialization;
				const auto& head            = materialization.Session.Binding.HeadOid;
				const auto  algorithm       = head.algorithm() == core::git_hash_algorithm::sha1
					? git_hash_algorithm::sha1
					: git_hash_algorithm::sha256;
				const auto  isolation       = canonical_existing(IsolationRoot);
				const auto  parent          = canonical_existing(materialization.ParentDirectory);
				if (
					!application::valid(materialization)
					|| (head.algorithm() != core::git_hash_algorithm::sha1
						&& head.algorithm() != core::git_hash_algorithm::sha256)
					|| !valid_repository(aRequest.Repository, algorithm)
					|| !isolation.has_value() || !parent.has_value()
					|| !private_directory(*parent)
					|| path_begins_with(*parent, *isolation)
					|| path_begins_with(*isolation, *parent)) {
					return std::unexpected{ git_process_failure::invalid_request };
				}

				const auto headText = std::string{ core::canonical_hex(head).view() };
				const auto name     = "bpr-agent-"
					+ std::to_string(materialization.ApplicationSession.value()) + "-"
					+ std::to_string(materialization.Session.Id.value()) + "-"
					+ headText.substr(0U, 16U);
				const auto      destination = (*parent / name).lexically_normal();
				std::error_code presenceError;
				const auto      destinationStatus = std::filesystem::symlink_status(
					destination,
					presenceError);
				if (
					destination.parent_path() != *parent
					|| (presenceError
						&& presenceError
							!= std::errc::no_such_file_or_directory)
					|| (!presenceError
						&& destinationStatus.type()
							!= std::filesystem::file_type::not_found)) {
					return std::unexpected{ git_process_failure::invalid_request };
				}

				if (!make_private_directory(destination)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}
				const auto identity = exact_identity(
					destination,
					exact_artifact_kind::directory);
				if (!identity.has_value()) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}
				auto       ownedPath = destination;
				const auto cleanup   = [&]() noexcept {
					return quarantine_and_remove_exact(
						ownedPath,
						*identity,
						true);
				};
				const auto fail = [&](git_process_error aFailure)
					-> git_process_result<application::owned_agent_workspace> {
					static_cast<void>(cleanup());
					return std::unexpected{ std::move(aFailure) };
				};

				auto initArguments = std::vector<std::string>{
					"-c",
					"core.fsync=all",
					"-c",
					"core.fsyncMethod=fsync",
					"-c",
					"core.hooksPath=" + path_text(HooksRoot),
					"-c",
					"credential.helper=",
					"-c",
					"credential.interactive=never",
					"-c",
					"submodule.recurse=false",
					"-c",
					"filter.lfs.required=false",
					"-c",
					"filter.lfs.smudge=",
					"-c",
					"filter.lfs.clean=",
					"init",
					"--quiet",
					"--template=" + path_text(TemplatesRoot),
				};
				if (algorithm == git_hash_algorithm::sha256) {
					initArguments.emplace_back("--object-format=sha256");
				}
				append_arguments(initArguments, "--", path_text(destination));
				const auto initialized = submit(
					process_request{
						.Arguments = std::move(initArguments),
						.Timeout   = pack_process_timeout,
					},
					aStop);
				if (!initialized.has_value()) {
					return fail(initialized.error());
				}

				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"pack-objects",
					"--revs",
					"--delta-base-offset",
					"--non-empty",
					"--",
					path_text(destination / ".git" / "objects" / "pack" / "pack"));
				const auto packed = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = memory_input{ headText + "\n" },
						.Timeout   = pack_process_timeout,
					},
					aStop);
				const auto packHashBytes = algorithm == git_hash_algorithm::sha1 ? 40U : 64U;
				if (
					!packed.has_value()
					|| packed->StandardOutput.size() != packHashBytes + 1U
					|| packed->StandardOutput.back() != '\n'
					|| !valid_fixed_hex(
						std::string_view{ packed->StandardOutput }.substr(0U, packHashBytes),
						packHashBytes)) {
					return fail(
						packed.has_value()
							? git_process_failure::malformed_output
							: packed.error());
				}

				arguments = workspace_git_arguments(destination, HooksRoot);
				append_arguments(arguments, "read-tree", "--reset", headText);
				const auto indexed = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Timeout   = pack_process_timeout,
					},
					aStop);
				if (!indexed.has_value()) {
					return fail(indexed.error());
				}
				arguments = workspace_git_arguments(destination, HooksRoot);
				append_arguments(arguments, "checkout-index", "--all", "--force");
				const auto materialized = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Timeout   = pack_process_timeout,
					},
					aStop);
				if (!materialized.has_value()) {
					return fail(materialized.error());
				}
				if (!write_private_file(destination / ".git" / "HEAD", headText + "\n")) {
					return fail(git_process_failure::invalid_repository);
				}

				std::error_code hooksError;
				static_cast<void>(std::filesystem::remove_all(
					destination / ".git" / "hooks",
					hooksError));
				if (hooksError) {
					return fail(git_process_failure::invalid_repository);
				}

				arguments = workspace_git_arguments(destination, HooksRoot);
				append_arguments(arguments, "rev-parse", "--verify", "HEAD^{commit}");
				const auto verifiedHead = submit(
					process_request{ .Arguments = std::move(arguments) },
					aStop);
				if (
					!verifiedHead.has_value()
					|| verifiedHead->StandardOutput != headText + "\n") {
					return fail(
						verifiedHead.has_value()
							? git_process_failure::malformed_output
							: verifiedHead.error());
				}

				arguments = workspace_git_arguments(destination, HooksRoot);
				append_arguments(arguments, "config", "--local", "--get-regexp", "^remote\\.");
				const auto remotes = submit(
					process_request{
						.Arguments    = std::move(arguments),
						.AllowExitOne = true,
					},
					aStop);
				if (
					!remotes.has_value() || remotes->ExitCode != 1
					|| !remotes->StandardOutput.empty()) {
					return fail(
						remotes.has_value()
							? git_process_failure::invalid_repository
							: remotes.error());
				}

				std::error_code topologyError;
				const auto      unsafeTopology = std::filesystem::exists(
													 destination / ".git" / "objects" / "info" / "alternates",
													 topologyError)
					|| topologyError
					|| std::filesystem::exists(
												destination / ".git" / "objects" / "info" / "http-alternates",
												topologyError)
					|| topologyError
					|| std::filesystem::exists(
												destination / ".git" / "worktrees",
												topologyError)
					|| topologyError
					|| std::filesystem::exists(
												destination / ".git" / "hooks",
												topologyError)
					|| topologyError;
				if (
					unsafeTopology || !private_directory(destination / ".git")
					|| !read_exact_file(destination / ".git" / "HEAD", headText + "\n")) {
					return fail(git_process_failure::invalid_repository);
				}

				const auto marker = "bpr-agent-workspace-v1:"
					+ std::to_string(materialization.ApplicationSession.value()) + ":"
					+ std::to_string(materialization.Session.Id.value()) + ":" + headText
					+ "\n";
				if (!write_private_file(destination / ".git" / "bpr-owned-workspace", marker)) {
					return fail(git_process_failure::invalid_repository);
				}
				return application::owned_agent_workspace{
					.Workspace = application::prepared_workspace{
						.Policy      = core::agent_workspace_policy::disposable_review_workspace,
						.PinnedHead  = head,
						.Path        = path_text(destination),
						.Cleanliness = application::checkout_cleanliness::clean,
					},
					.ApplicationSession = materialization.ApplicationSession,
					.Authority          = materialization.Authority,
					.Session            = materialization.Session.Id,
					.ParentDirectory    = *parent,
					.OwnershipMarker    = marker,
					.DirectoryIdentity  = workspace_directory_identity(*identity),
				};
			}

			[[nodiscard]] auto cleanup_agent_workspace(
				const application::owned_agent_workspace& aWorkspace,
				const std::stop_token                     aStop)
				-> git_process_result<void> override
			{
				if (aStop.stop_requested()) {
					return std::unexpected{ git_process_failure::cancelled };
				}
				auto boundary = process_boundary_lock{ IsolationRoot, aStop };
				if (!boundary.valid()) {
					return std::unexpected{
						aStop.stop_requested()
							? git_process_failure::cancelled
							: git_process_failure::invalid_configuration
					};
				}
				const auto isolation   = canonical_existing(IsolationRoot);
				const auto parent      = canonical_existing(aWorkspace.ParentDirectory);
				const auto destination = resolve_canonical_existing(
					std::filesystem::path{ aWorkspace.Workspace.Path });
				if (
					!application::valid(aWorkspace) || !isolation.has_value()
					|| !parent.has_value() || !destination.has_value()
					|| !private_directory(*parent) || !private_directory(*destination)
					|| destination->parent_path() != *parent
					|| path_begins_with(*parent, *isolation)
					|| path_begins_with(*isolation, *parent)
					|| !read_exact_file(
						*destination / ".git" / "bpr-owned-workspace",
						aWorkspace.OwnershipMarker)) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				const auto identity = exact_directory_identity(
					aWorkspace.DirectoryIdentity);
				if (!identity.has_value()) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				auto ownedPath = *destination;
				if (!quarantine_and_remove_exact(ownedPath, *identity, true)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}
				return { };
			}

			[[nodiscard]] auto materialize_blob(
				const application::git_blob_request&        aRequest,
				application::git_blob_materialization_sink& aSink,
				const std::stop_token                       aStop)
				-> git_process_result<void> override
			{
				if (
					!application::valid(aRequest.Blob)
					|| aRequest.MaximumBytes == 0U
					|| aRequest.MaximumBytes
						> application::maximum_git_blob_materialization_bytes) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				if (!valid_repository(aRequest.Repository, aRequest.Blob.Algorithm)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}

				const auto blob      = object_text(aRequest.Blob);
				auto       arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"cat-file",
					"--batch-check=%(objectname) %(objecttype) %(objectsize)");
				const auto checked = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = memory_input{ blob + "\n" },
					},
					aStop);
				if (!checked.has_value()) {
					return std::unexpected{ checked.error() };
				}
				if (checked->StandardOutput == blob + " missing\n") {
					return std::unexpected{ git_process_failure::object_unavailable };
				}
				const auto prefix = blob + " blob ";
				if (
					!checked->StandardOutput.starts_with(prefix)
					|| checked->StandardOutput.empty()
					|| checked->StandardOutput.back() != '\n') {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				const auto sizeText = std::string_view{ checked->StandardOutput }.substr(
					prefix.size(),
					checked->StandardOutput.size() - prefix.size() - 1U);
				std::uint64_t size      = 0U;
				const auto [end, error] = std::from_chars(
					sizeText.data(), sizeText.data() + sizeText.size(), size);
				if (
					error != std::errc{ } || end != sizeText.data() + sizeText.size()
					|| size > aRequest.MaximumBytes
					|| size > static_cast<std::uint64_t>(
						   std::numeric_limits<std::size_t>::max())) {
					return std::unexpected{
						size > aRequest.MaximumBytes
							? git_process_failure::standard_output_limit
							: git_process_failure::malformed_output
					};
				}

				arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(arguments, "cat-file", "blob", blob);
				const auto content = submit(
					process_request{
						.Arguments           = std::move(arguments),
						.StandardOutputLimit = static_cast<std::size_t>(size),
					},
					aStop);
				if (!content.has_value()) {
					return std::unexpected{ content.error() };
				}
				if (content->StandardOutput.size() != static_cast<std::size_t>(size)) {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				aSink.accept(
					aRequest.Blob,
					std::as_bytes(std::span{ content->StandardOutput }));
				return { };
			}

			[[nodiscard]] auto find_blob_at_path(
				const application::git_tree_entry_request& aRequest,
				const std::stop_token                      aStop)
				-> git_process_result<std::optional<application::git_blob_id>> override
			{
				if (!application::valid(aRequest.Commit)
					|| !valid_tree_entry_path(aRequest.Path)) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				if (!valid_repository(aRequest.Repository, aRequest.Commit.Algorithm)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}

				const auto requestedPath = tree_entry_path_text(aRequest.Path);
				auto       arguments     = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"ls-tree",
					"-z",
					"--full-tree",
					object_text(aRequest.Commit),
					"--",
					":(literal)" + requestedPath);
				const auto listed = submit(
					process_request{
						.Arguments = std::move(arguments),
						.StandardOutputLimit
						= application::maximum_git_tree_entry_path_bytes + 128U,
					},
					aStop);
				if (!listed.has_value()) {
					return std::unexpected{ listed.error() };
				}
				if (listed->StandardOutput.empty()) {
					return std::optional<application::git_blob_id>{ };
				}
				const auto output = std::string_view{ listed->StandardOutput };
				if (output.back() != '\0'
					|| output.find('\0') != output.size() - 1U) {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				const auto record = output.substr(0U, output.size() - 1U);
				const auto tab    = record.find('\t');
				if (tab == std::string_view::npos
					|| record.substr(tab + 1U) != requestedPath) {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				const auto header     = record.substr(0U, tab);
				const auto firstSpace = header.find(' ');
				const auto lastSpace  = header.rfind(' ');
				if (firstSpace == std::string_view::npos || firstSpace == lastSpace
					|| (header.substr(0U, firstSpace) != "100644"
						&& header.substr(0U, firstSpace) != "100755"
						&& header.substr(0U, firstSpace) != "120000")
					|| header.substr(firstSpace + 1U, lastSpace - firstSpace - 1U)
						!= "blob") {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				const auto blob = parse_object<application::git_blob_id>(
					header.substr(lastSpace + 1U),
					aRequest.Commit.Algorithm);
				if (!blob.has_value()) {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				return blob;
			}

			[[nodiscard]] auto find_rename_pairs(
				const application::git_rename_pair_request& aRequest,
				const std::stop_token                       aStop)
				-> git_process_result<application::git_rename_pair_observation> override
			{
				const auto algorithm = aRequest.Left.Algorithm;
				if (!application::valid(aRequest.Left)
					|| !application::valid(aRequest.Right)
					|| aRequest.Right.Algorithm != algorithm
					|| !valid_tree_entry_path(aRequest.CurrentPath)
					|| (aRequest.PreviousPath.has_value()
						&& !valid_tree_entry_path(*aRequest.PreviousPath))) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				if (!valid_repository(aRequest.Repository, algorithm)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}

				const auto currentPath  = tree_entry_path_text(aRequest.CurrentPath);
				const auto previousPath = aRequest.PreviousPath.has_value()
					? std::optional{ tree_entry_path_text(*aRequest.PreviousPath) }
					: std::nullopt;
				auto       arguments    = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"diff-tree",
					"-r",
					"-z",
					"--no-commit-id",
					"--name-status",
					"--diff-filter=R",
					"--find-renames=50%",
					object_text(aRequest.Left),
					object_text(aRequest.Right));
				constexpr auto recordLimit
					= application::maximum_git_tree_entry_path_bytes * 2U + 8U;
				const auto renamed = submit(
					process_request{
						.Arguments = std::move(arguments),
						.StandardOutputLimit
						= application::maximum_git_rename_scan_records * recordLimit,
					},
					aStop);
				if (!renamed.has_value()) {
					return std::unexpected{ renamed.error() };
				}

				auto       observation = application::git_rename_pair_observation{ };
				const auto output      = std::string_view{ renamed->StandardOutput };
				auto       offset      = std::size_t{ 0U };
				auto       scanned     = std::size_t{ 0U };
				const auto field       = [&]() -> std::optional<std::string_view> {
					const auto end = output.find('\0', offset);
					if (end == std::string_view::npos) {
						return std::nullopt;
					}
					const auto result = output.substr(offset, end - offset);
					offset            = end + 1U;
					return result;
				};
				while (offset < output.size()) {
					const auto status = field();
					const auto left   = field();
					const auto right  = field();
					if (!status.has_value() || !left.has_value() || !right.has_value()) {
						return std::unexpected{ git_process_failure::malformed_output };
					}
					++scanned;
					if (scanned > application::maximum_git_rename_scan_records) {
						return std::unexpected{ git_process_failure::standard_output_limit };
					}
					if (status->size() != 4U || status->front() != 'R' || left->empty()
						|| right->empty() || *left == *right) {
						return std::unexpected{ git_process_failure::malformed_output };
					}
					const auto score = status->substr(1U);
					if (!std::ranges::all_of(score, [](const char aValue) {
							return aValue >= '0' && aValue <= '9';
						})
						|| score < "050" || score > "100") {
						return std::unexpected{ git_process_failure::malformed_output };
					}
					const auto leftPath  = tree_entry_path(*left);
					const auto rightPath = tree_entry_path(*right);
					const auto bound     = *left == currentPath || *right == currentPath
						|| (previousPath.has_value()
							&& (*left == *previousPath || *right == *previousPath));
					if (!valid_tree_entry_path(leftPath)
						|| !valid_tree_entry_path(rightPath)) {
						return std::unexpected{ git_process_failure::malformed_output };
					}
					if (!bound) {
						continue;
					}
					if (observation.Count
						>= application::maximum_git_rename_pair_records) {
						return std::unexpected{ git_process_failure::standard_output_limit };
					}
					observation.Records[observation.Count++] = {
						.LeftPath  = leftPath,
						.RightPath = rightPath,
					};
				}
				if (offset != output.size()) {
					return std::unexpected{ git_process_failure::malformed_output };
				}
				return observation;
			}

			[[nodiscard]] auto is_ancestor(
				const application::git_ancestry_request& aRequest,
				const std::stop_token                    aStop)
				-> git_process_result<git_ancestry> override
			{
				if (
					!application::valid(aRequest.Ancestor)
					|| !application::valid(aRequest.Descendant)
					|| aRequest.Ancestor.Algorithm
						!= aRequest.Descendant.Algorithm) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (
					!valid_repository(
						aRequest.Repository,
						aRequest.Ancestor.Algorithm)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				const auto ancestor = inspect_commit(
					application::git_commit_request{
						.Repository = aRequest.Repository,
						.Commit     = aRequest.Ancestor,
					},
					aStop);
				if (!ancestor.has_value()) {
					return std::unexpected{ ancestor.error() };
				}
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"merge-base",
					"--is-ancestor",
					object_text(aRequest.Ancestor),
					object_text(aRequest.Descendant));
				const auto result = submit(
					process_request{
						.Arguments    = std::move(arguments),
						.AllowExitOne = true,
					},
					aStop);
				if (!result.has_value()) {
					return std::unexpected{ result.error() };
				}
				if (!result->StandardOutput.empty()) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				return result->ExitCode == 0
					? git_ancestry::ancestor
					: git_ancestry::not_ancestor;
			}

			[[nodiscard]] auto verify_connectivity(
				const application::git_connectivity_request& aRequest,
				const std::stop_token                        aStop)
				-> git_process_result<void> override
			{
				if (
					!application::valid(aRequest.Base)
					|| !application::valid(aRequest.Head)
					|| aRequest.Base.Algorithm != aRequest.Head.Algorithm) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (
					!valid_repository(
						aRequest.Repository,
						aRequest.Base.Algorithm)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"fsck",
					"--connectivity-only",
					"--no-dangling",
					"--no-reflogs",
					"--no-progress",
					"--no-name-objects",
					"--",
					object_text(aRequest.Base),
					object_text(aRequest.Head));
				const auto result = submit(
					process_request{ .Arguments = std::move(arguments) },
					aStop);
				if (!result.has_value()) {
					return std::unexpected{ result.error() };
				}
				if (!result->StandardOutput.empty()) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				return { };
			}

			[[nodiscard]] auto update_refs(
				const application::git_update_refs_request& aRequest,
				const std::stop_token                       aStop)
				-> git_process_result<void> override
			{
				const auto algorithm = mutation_algorithm(aRequest.Mutations);
				if (
					!algorithm.has_value()
					|| aRequest.Mutations.empty()
					|| aRequest.Mutations.size()
						> application::maximum_git_ref_transaction_updates) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (!valid_repository(aRequest.Repository, *algorithm)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				auto input = std::string{ "start" };
				input.push_back('\0');
				for (const auto& mutation : aRequest.Mutations) {
					const auto appended = std::visit(
						[&](const auto& aValue) {
							const auto ref = ref_text(aValue.Ref);
							if (!ref.has_value()) {
								return false;
							}
							using value_type = std::decay_t<decltype(aValue)>;
							if constexpr (
								std::is_same_v<
									value_type,
									application::git_ref_write>) {
								if (
									!application::valid(aValue.DesiredObject)
									|| aValue.DesiredObject.Algorithm
										!= *algorithm) {
									return false;
								}
								if (
									std::holds_alternative<
										application::git_ref_expected_absent>(
										aValue.ExpectedOld)) {
									input += "create " + *ref;
									input.push_back('\0');
									input += object_text(aValue.DesiredObject);
									input.push_back('\0');
									return true;
								}
								const auto expected = std::get<
														  application::git_ref_expected_object>(
														  aValue.ExpectedOld)
														  .Object;
								if (
									!application::valid(expected)
									|| expected.Algorithm != *algorithm) {
									return false;
								}
								input += "update " + *ref;
								input.push_back('\0');
								input += object_text(aValue.DesiredObject);
								input.push_back('\0');
								input += object_text(expected);
								input.push_back('\0');
								return true;
							} else {
								if (
									!application::valid(aValue.ExpectedOld)
									|| aValue.ExpectedOld.Algorithm
										!= *algorithm) {
									return false;
								}
								input += "delete " + *ref;
								input.push_back('\0');
								input += object_text(aValue.ExpectedOld);
								input.push_back('\0');
								return true;
							}
						},
						mutation);
					if (!appended) {
						return std::unexpected{
							git_process_failure::invalid_request
						};
					}
				}
				input += "prepare";
				input.push_back('\0');
				input += "commit";
				input.push_back('\0');

				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"update-ref",
					"--stdin",
					"-z",
					"--no-deref");
				const auto result = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = memory_input{ .Bytes = std::move(input) },
					},
					aStop);
				if (!result.has_value()) {
					return std::unexpected{ result.error() };
				}
				if (result->StandardOutput != transaction_success) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				return { };
			}

			[[nodiscard]] auto inspect_ref(
				const application::git_inspect_ref_request& aRequest,
				const std::stop_token                       aStop)
				-> git_process_result<std::optional<git_commit_id>> override
			{
				const auto target = inspect_ref_target(aRequest, aStop);
				if (!target.has_value() || !target->has_value()) {
					return target;
				}
				const auto inspection = inspect_commit(
					application::git_commit_request{
						.Repository = aRequest.Repository,
						.Commit     = **target,
					},
					aStop);
				if (!inspection.has_value()) {
					return std::unexpected{ inspection.error() };
				}
				return target;
			}

			[[nodiscard]] auto inspect_ref_target(
				const application::git_inspect_ref_request& aRequest,
				const std::stop_token                       aStop)
				-> git_process_result<std::optional<git_commit_id>> override
			{
				const auto ref = ref_text(aRequest.Ref);
				if (!ref.has_value()) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				const auto algorithm = repository_algorithm(aRequest.Repository);
				if (!algorithm.has_value()) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(
					arguments,
					"for-each-ref",
					"--format=%(refname)%00%(objectname)%00%(symref)",
					"--count=2",
					*ref);
				const auto result = submit(
					process_request{ .Arguments = std::move(arguments) },
					aStop);
				if (!result.has_value()) {
					return std::unexpected{ result.error() };
				}
				if (result->StandardOutput.empty()) {
					return std::optional<git_commit_id>{ };
				}
				const auto nameSeparator   = result->StandardOutput.find('\0');
				const auto objectSeparator = nameSeparator == std::string::npos
					? std::string::npos
					: result->StandardOutput.find('\0', nameSeparator + 1U);
				if (
					nameSeparator == std::string::npos
					|| objectSeparator == std::string::npos
					|| std::string_view{ result->StandardOutput }.substr(
						   0U,
						   nameSeparator)
						!= *ref
					|| result->StandardOutput.back() != '\n'
					|| result->StandardOutput.find('\n')
						!= result->StandardOutput.size() - 1U
					|| objectSeparator + 1U
						!= result->StandardOutput.size() - 1U) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				const auto object = parse_object<git_commit_id>(
					std::string_view{ result->StandardOutput }.substr(
						nameSeparator + 1U,
						objectSeparator - nameSeparator - 1U),
					*algorithm);
				if (!object.has_value()) {
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				return std::optional<git_commit_id>{ object };
			}

			[[nodiscard]] auto capability_probe(std::stop_token aStop)
				-> git_process_result<void>;

		private:
			[[nodiscard]] auto initialize_owned_bare_repository(
				const application::git_initialize_repository_request& aRequest,
				const std::stop_token                                 aStop)
				-> git_process_result<exact_artifact_identity>
			{
				if (
					!valid_hash_algorithm(aRequest.Algorithm)
					|| !valid_new_repository_path(aRequest.Repository)) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				std::error_code createError;
#if defined(Q_OS_UNIX)
				const auto created = ::mkdir(aRequest.Repository.c_str(), S_IRWXU) == 0;
				if (!created) {
					createError = std::error_code{ errno, std::generic_category() };
				}
#else
				const auto created = std::filesystem::create_directory(
					aRequest.Repository,
					createError);
#endif
				if (!created || createError) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				const auto identity = exact_identity(
					aRequest.Repository,
					exact_artifact_kind::directory);
				if (
					!identity.has_value()
					|| !private_directory(aRequest.Repository)) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				auto       ownedPath = aRequest.Repository;
				const auto cleanupRepository =
					[&]() noexcept {
						return quarantine_and_remove_exact(
							ownedPath,
							*identity,
							true);
					};
				auto arguments = std::vector<std::string>{
					"init",
					"--bare",
					"--quiet",
					"--initial-branch=main",
					aRequest.Algorithm == git_hash_algorithm::sha1
						? "--object-format=sha1"
						: "--object-format=sha256",
					"--template=" + path_text(TemplatesRoot),
					path_text(aRequest.Repository),
				};
				const auto result = submit(
					process_request{
						.Arguments = std::move(arguments),
						.Input     = memory_input{ },
						.Timeout   = process_timeout,
					},
					aStop);
				if (!result.has_value()) {
					if (
						result.error()
							!= git_process_failure::reap_failure
						&& !cleanupRepository()) {
						return std::unexpected{
							git_process_failure::invalid_repository
						};
					}
					return std::unexpected{ result.error() };
				}
				if (!result->StandardOutput.empty()) {
					if (!cleanupRepository()) {
						return std::unexpected{
							git_process_failure::invalid_repository
						};
					}
					return std::unexpected{
						git_process_failure::malformed_output
					};
				}
				if (
					exact_identity(
						aRequest.Repository,
						exact_artifact_kind::directory)
					!= identity) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				std::error_code permissionError;
				std::filesystem::permissions(
					aRequest.Repository,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace,
					permissionError);
				const auto config = repository_config(aRequest.Algorithm);
				if (
					permissionError
					|| !write_private_file(
						aRequest.Repository / "config",
						config)
					|| !write_private_file(
						aRequest.Repository / "HEAD",
						"ref: refs/heads/main\n")
					|| exact_identity(
						   aRequest.Repository,
						   exact_artifact_kind::directory)
						!= identity
					|| !valid_repository(
						aRequest.Repository,
						aRequest.Algorithm)) {
					static_cast<void>(cleanupRepository());
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				return *identity;
			}

			[[nodiscard]] auto submit(
				process_request       aRequest,
				const std::stop_token aStop) -> process_result
			{
				if (
					!empty_private_directory(TemplatesRoot)
					|| !empty_private_directory(HooksRoot)) {
					return std::unexpected{
						git_process_failure::invalid_configuration
					};
				}
				return Worker->submit(std::move(aRequest), aStop);
			}

			[[nodiscard]] auto valid_new_repository_path(
				const std::filesystem::path& aRepository) const -> bool
			{
				if (
					aRepository.empty() || !aRepository.is_absolute()
					|| aRepository.parent_path() != RepositoriesRoot) {
					return false;
				}
				const auto name = aRepository.filename().string();
				if (
					name.size() != 68U
					|| name.substr(name.size() - 4U) != ".git"
					|| !valid_fixed_hex(name.substr(0U, 64U), 64U)) {
					return false;
				}
				std::error_code error;
				return !std::filesystem::exists(aRepository, error) && !error
					&& private_directory(RepositoriesRoot);
			}

			[[nodiscard]] auto repository_algorithm(
				const std::filesystem::path& aRepository) const
				-> std::optional<git_hash_algorithm>
			{
				if (valid_repository(aRepository, git_hash_algorithm::sha1)) {
					return git_hash_algorithm::sha1;
				}
				if (valid_repository(aRepository, git_hash_algorithm::sha256)) {
					return git_hash_algorithm::sha256;
				}
				return std::nullopt;
			}

			[[nodiscard]] auto valid_repository(
				const std::filesystem::path& aRepository,
				const git_hash_algorithm     aAlgorithm) const -> bool
			{
				if (
					!valid_hash_algorithm(aAlgorithm)
					|| aRepository.parent_path() != RepositoriesRoot) {
					return false;
				}
				const auto canonical = canonical_existing(aRepository);
				if (!canonical.has_value() || !private_directory(*canonical)) {
					return false;
				}
				const auto name = aRepository.filename().string();
				if (
					name.size() != 68U
					|| name.substr(name.size() - 4U) != ".git"
					|| !valid_fixed_hex(name.substr(0U, 64U), 64U)) {
					return false;
				}
				std::error_code error;
				if (
					!private_directory(aRepository / "objects")
					|| !private_directory(aRepository / "refs")
					|| !read_exact_file(
						aRepository / "config",
						repository_config(aAlgorithm))
					|| !read_exact_file(
						aRepository / "HEAD",
						"ref: refs/heads/main\n")
					|| std::filesystem::exists(
						aRepository / "objects" / "info" / "alternates",
						error)
					|| error
					|| std::filesystem::exists(
						aRepository
							/ "objects" / "info" / "http-alternates",
						error)
					|| error
					|| std::filesystem::exists(
						aRepository / "info" / "grafts",
						error)
					|| error
					|| std::filesystem::exists(
						aRepository / "refs" / "replace",
						error)
					|| error
					|| std::filesystem::exists(
						aRepository / "shallow",
						error)
					|| error
					|| std::filesystem::exists(
						aRepository / "worktrees",
						error)
					|| error) {
					return false;
				}
				return true;
			}

			[[nodiscard]] static auto mutation_algorithm(
				const std::span<const application::git_ref_mutation> aMutations)
				-> std::optional<git_hash_algorithm>
			{
				if (aMutations.empty()) {
					return std::nullopt;
				}
				return std::visit(
					[](const auto& aMutation)
						-> std::optional<git_hash_algorithm> {
						using mutation_type = std::decay_t<decltype(aMutation)>;
						if constexpr (
							std::is_same_v<
								mutation_type,
								application::git_ref_write>) {
							return aMutation.DesiredObject.Algorithm;
						} else {
							return aMutation.ExpectedOld.Algorithm;
						}
					},
					aMutations.front());
			}

			application::git_installation   Installation;
			std::filesystem::path           IsolationRoot;
			std::filesystem::path           RepositoriesRoot;
			std::filesystem::path           IncomingRoot;
			std::filesystem::path           TemplatesRoot;
			std::filesystem::path           HooksRoot;
			std::unique_ptr<process_worker> Worker;
		};

		inline constexpr std::array<std::uint8_t, 230U> probe_pack{
			0x50,
			0x41,
			0x43,
			0x4b,
			0x00,
			0x00,
			0x00,
			0x02,
			0x00,
			0x00,
			0x00,
			0x03,
			0x97,
			0x0b,
			0x78,
			0x9c,
			0x95,
			0xcb,
			0xc1,
			0x0a,
			0x02,
			0x21,
			0x10,
			0x80,
			0xe1,
			0xbb,
			0x4f,
			0x31,
			0xf7,
			0x20,
			0x74,
			0x35,
			0xdb,
			0x85,
			0x58,
			0xa2,
			0x4b,
			0xd7,
			0xe8,
			0x0d,
			0x46,
			0x1d,
			0x69,
			0xc0,
			0x5d,
			0xc5,
			0xa6,
			0xa8,
			0xb7,
			0xaf,
			0x1e,
			0xa1,
			0xff,
			0xfc,
			0xfd,
			0xd2,
			0x89,
			0xc0,
			0x45,
			0x9a,
			0x72,
			0x0e,
			0x79,
			0x1f,
			0x63,
			0xb2,
			0x26,
			0x51,
			0x08,
			0x1a,
			0xad,
			0xb3,
			0x7a,
			0x18,
			0x8c,
			0x35,
			0x59,
			0xe7,
			0x44,
			0xde,
			0x91,
			0xdd,
			0x25,
			0x85,
			0x0f,
			0xb9,
			0xd5,
			0x0e,
			0xa7,
			0xcb,
			0x15,
			0x0e,
			0xa1,
			0xf5,
			0x23,
			0xbd,
			0x70,
			0x69,
			0x85,
			0xb6,
			0xbc,
			0x3e,
			0xb1,
			0x70,
			0x9a,
			0x61,
			0x72,
			0xde,
			0x8f,
			0x6e,
			0xd4,
			0x1a,
			0x36,
			0xfa,
			0x9b,
			0x8a,
			0x75,
			0x59,
			0x58,
			0x84,
			0xfe,
			0x79,
			0xd4,
			0x8f,
			0xf2,
			0x7a,
			0x17,
			0x2c,
			0x85,
			0x12,
			0x9c,
			0x59,
			0x20,
			0x62,
			0xc3,
			0xc0,
			0x85,
			0xe5,
			0x0d,
			0xad,
			0xd7,
			0x40,
			0xea,
			0x03,
			0x67,
			0xd5,
			0x39,
			0x11,
			0xa5,
			0x02,
			0x78,
			0x9c,
			0x33,
			0x34,
			0x30,
			0x30,
			0x33,
			0x31,
			0x51,
			0x28,
			0x28,
			0xca,
			0x4f,
			0x4a,
			0xd5,
			0x2b,
			0xa9,
			0x28,
			0x61,
			0xb8,
			0xc5,
			0xe3,
			0xb7,
			0xe3,
			0xe6,
			0x92,
			0x5e,
			0x71,
			0x29,
			0xe3,
			0x70,
			0x6f,
			0x0b,
			0xf6,
			0xa0,
			0x87,
			0xcd,
			0x1a,
			0xe9,
			0x81,
			0x00,
			0xe4,
			0xb2,
			0x0c,
			0xc7,
			0x36,
			0x78,
			0x9c,
			0x2b,
			0x28,
			0xca,
			0x4f,
			0x4a,
			0xe5,
			0x02,
			0x00,
			0x08,
			0x96,
			0x02,
			0x23,
			0xe2,
			0x48,
			0xb0,
			0x8f,
			0x09,
			0xa7,
			0x51,
			0x16,
			0xfd,
			0xb2,
			0x39,
			0x7c,
			0x92,
			0x8a,
			0x84,
			0xf6,
			0xa9,
			0x3f,
			0x6c,
			0x34,
		};

		inline constexpr std::array<std::uint8_t, 271U> probe_pack_sha256{
			0x50,
			0x41,
			0x43,
			0x4b,
			0x00,
			0x00,
			0x00,
			0x02,
			0x00,
			0x00,
			0x00,
			0x03,
			0x9f,
			0x0c,
			0x78,
			0x9c,
			0x95,
			0xcb,
			0x41,
			0x0a,
			0xc2,
			0x30,
			0x10,
			0x40,
			0xd1,
			0x7d,
			0x4e,
			0x31,
			0x7b,
			0x41,
			0x92,
			0x36,
			0x0d,
			0x13,
			0x10,
			0x29,
			0x6e,
			0xdc,
			0x8a,
			0x37,
			0x98,
			0x24,
			0x33,
			0x18,
			0x48,
			0xdb,
			0x10,
			0xa3,
			0xe8,
			0xed,
			0xd5,
			0x23,
			0xf8,
			0xd7,
			0xef,
			0xf7,
			0xc6,
			0x0c,
			0xc9,
			0x04,
			0x43,
			0xe8,
			0xc8,
			0x85,
			0xc9,
			0xe0,
			0x30,
			0x68,
			0x14,
			0xd1,
			0xa3,
			0x9d,
			0x44,
			0x9c,
			0x38,
			0x9b,
			0x7c,
			0xb4,
			0x18,
			0x7c,
			0xd2,
			0x13,
			0xb2,
			0x66,
			0x24,
			0x44,
			0x09,
			0xc4,
			0x12,
			0xc5,
			0xf8,
			0x61,
			0x44,
			0x4b,
			0x84,
			0x49,
			0xd1,
			0xa3,
			0xdf,
			0xb6,
			0x06,
			0xa7,
			0xcb,
			0x15,
			0x0e,
			0xa1,
			0xb6,
			0x99,
			0x5f,
			0xb4,
			0xd4,
			0xc2,
			0xfb,
			0xbc,
			0x3e,
			0xa9,
			0xe4,
			0x74,
			0x04,
			0x6f,
			0x9d,
			0x43,
			0x8b,
			0x5a,
			0xc3,
			0x4e,
			0x7f,
			0x53,
			0x71,
			0x5b,
			0x96,
			0xdc,
			0x3b,
			0xff,
			0xf3,
			0xa8,
			0x1f,
			0xcd,
			0xeb,
			0xbd,
			0x53,
			0x29,
			0x9c,
			0xe0,
			0x9c,
			0x3b,
			0x44,
			0xaa,
			0x14,
			0x72,
			0xc9,
			0xfd,
			0x0d,
			0xb5,
			0x6d,
			0x81,
			0xd5,
			0x07,
			0x40,
			0x7d,
			0x3f,
			0x67,
			0xa1,
			0x03,
			0x78,
			0x9c,
			0x01,
			0x31,
			0x00,
			0xce,
			0xff,
			0x31,
			0x30,
			0x30,
			0x36,
			0x34,
			0x34,
			0x20,
			0x70,
			0x72,
			0x6f,
			0x62,
			0x65,
			0x2e,
			0x74,
			0x78,
			0x74,
			0x00,
			0x73,
			0x32,
			0x45,
			0xb5,
			0xd4,
			0x45,
			0x55,
			0x8d,
			0x5e,
			0xd9,
			0xc6,
			0x98,
			0x92,
			0xdb,
			0x5c,
			0xac,
			0xb1,
			0x8a,
			0x05,
			0xc6,
			0xde,
			0x8f,
			0x70,
			0xd8,
			0xf1,
			0x3b,
			0x76,
			0x2b,
			0x3d,
			0xa2,
			0x20,
			0xa1,
			0xde,
			0xfa,
			0x15,
			0xc2,
			0x36,
			0x78,
			0x9c,
			0x2b,
			0x28,
			0xca,
			0x4f,
			0x4a,
			0xe5,
			0x02,
			0x00,
			0x08,
			0x96,
			0x02,
			0x23,
			0x3e,
			0x34,
			0x68,
			0x38,
			0x8a,
			0x66,
			0xc6,
			0x47,
			0x5d,
			0x2c,
			0x1f,
			0xdb,
			0x8f,
			0xb7,
			0x28,
			0x6b,
			0xc0,
			0xae,
			0x1f,
			0x93,
			0x4b,
			0x63,
			0x52,
			0x35,
			0x8c,
			0xc9,
			0x8e,
			0x4e,
			0x5e,
			0x82,
			0x3b,
			0x69,
		};

		[[nodiscard]] auto probe_identity(
			const std::string_view   aHex,
			const git_hash_algorithm aAlgorithm) -> application::git_commit_id
		{
			return *parse_object<application::git_commit_id>(
				aHex,
				aAlgorithm);
		}

		[[nodiscard]] auto probe_pack_identity(
			const std::string_view   aHex,
			const git_hash_algorithm aAlgorithm) -> application::git_pack_id
		{
			return *parse_object<application::git_pack_id>(
				aHex,
				aAlgorithm);
		}

		[[nodiscard]] auto probe_tree_identity(
			const std::string_view   aHex,
			const git_hash_algorithm aAlgorithm) -> application::git_tree_id
		{
			return *parse_object<application::git_tree_id>(
				aHex,
				aAlgorithm);
		}

		class exact_probe_cleanup final
		{
		public:
			exact_probe_cleanup(
				std::filesystem::path aRepository,
				std::filesystem::path aSpool)
				: Repository{ std::move(aRepository) }
				, Spool{ std::move(aSpool) }
			{
			}

			exact_probe_cleanup(const exact_probe_cleanup&) = delete;
			auto operator=(const exact_probe_cleanup&)
				-> exact_probe_cleanup& = delete;

			~exact_probe_cleanup()
			{
				if (!Complete && !Retained) {
					static_cast<void>(cleanup());
				}
			}

			auto repository_created(
				exact_artifact_identity aIdentity) noexcept -> void
			{
				RepositoryIdentity = std::move(aIdentity);
			}

			auto spool_created(
				exact_artifact_identity aIdentity) noexcept -> void
			{
				SpoolIdentity = std::move(aIdentity);
			}

			auto retain() noexcept -> void
			{
				Retained = true;
			}

			[[nodiscard]] auto cleanup() noexcept -> bool
			{
				auto spoolClean = true;
				if (SpoolIdentity.has_value()) {
					spoolClean = quarantine_and_remove_exact(
						Spool,
						*SpoolIdentity,
						false);
					if (spoolClean) {
						SpoolIdentity.reset();
					}
				}
				auto repositoryClean = true;
				if (RepositoryIdentity.has_value()) {
					repositoryClean = quarantine_and_remove_exact(
						Repository,
						*RepositoryIdentity,
						true);
					if (repositoryClean) {
						RepositoryIdentity.reset();
					}
				}
				Complete = spoolClean && repositoryClean;
				return Complete;
			}

		private:
			std::filesystem::path                  Repository;
			std::filesystem::path                  Spool;
			std::optional<exact_artifact_identity> RepositoryIdentity;
			std::optional<exact_artifact_identity> SpoolIdentity;
			bool                                   Retained = false;
			bool                                   Complete = false;
		};

		auto installed_git_process::capability_probe(
			const std::stop_token aStop)
			-> git_process_result<void>
		{
			auto boundary = process_boundary_lock{ IsolationRoot, aStop };
			if (!boundary.valid()) {
				return std::unexpected{
					aStop.stop_requested()
						? git_process_failure::cancelled
						: git_process_failure::invalid_configuration
				};
			}
			const auto probe =
				[&](const git_hash_algorithm            aAlgorithm,
					const std::span<const std::uint8_t> aPackBytes,
					const std::string_view              aPackHex,
					const std::string_view              aCommitHex,
					const std::string_view              aTreeHex)
				-> git_process_result<void> {
				if (aStop.stop_requested()) {
					return std::unexpected{
						git_process_failure::cancelled
					};
				}
				std::random_device random;
				const auto         presence =
					[](const std::filesystem::path& aPath)
					-> std::optional<bool> {
					std::error_code error;
					const auto      status = std::filesystem::symlink_status(aPath, error);
					if (
						error
						&& error
							!= std::errc::no_such_file_or_directory) {
						return std::nullopt;
					}
					return !error
						&& status.type()
						!= std::filesystem::file_type::not_found;
				};
				application::git_operation_identity  operation{ };
				std::filesystem::path                repository;
				std::filesystem::path                expectedSpool;
				std::unique_ptr<exact_probe_cleanup> cleanup;
				constexpr std::size_t                maximumProbePathAttempts = 8U;
				for (
					std::size_t attempt = 0U;
					attempt < maximumProbePathAttempts;
					++attempt) {
					if (aStop.stop_requested()) {
						return std::unexpected{
							git_process_failure::cancelled
						};
					}
					for (auto& byte : operation.Bytes) {
						byte = static_cast<std::uint8_t>(random());
					}
					if (!application::valid_identity(operation)) {
						operation.Bytes.front() = 1U;
					}
#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
					std::error_code collisionMarkerError;
					if (
						attempt == 0U
						&& std::filesystem::exists(
							IsolationRoot / "inject-probe-collision",
							collisionMarkerError)
						&& !collisionMarkerError) {
						operation.Bytes.fill(0x0cU);
					}
#endif
					repository = RepositoriesRoot
						/ (lower_hex(operation.Bytes) + ".git");
					expectedSpool = IncomingRoot
						/ (lower_hex(operation.Bytes) + ".pack.part");
					const auto repositoryBefore = presence(repository);
					const auto spoolBefore      = presence(expectedSpool);
					if (
						!repositoryBefore.has_value()
						|| !spoolBefore.has_value()) {
						return std::unexpected{
							git_process_failure::invalid_configuration
						};
					}
					if (*repositoryBefore || *spoolBefore) {
						continue;
					}

					auto candidateCleanup = std::make_unique<exact_probe_cleanup>(
						repository,
						expectedSpool);
					const auto spoolWrite = write_new_private_file(
						expectedSpool,
						std::as_bytes(aPackBytes));
					if (!spoolWrite.Created) {
						continue;
					}
					if (!spoolWrite.Identity.has_value()) {
						candidateCleanup->retain();
						return std::unexpected{
							git_process_failure::invalid_repository
						};
					}
					candidateCleanup->spool_created(
						*spoolWrite.Identity);
					if (!spoolWrite.Complete) {
						return std::unexpected{
							git_process_failure::standard_input
						};
					}
					if (
						exact_identity(
							expectedSpool,
							exact_artifact_kind::regular_file)
						!= spoolWrite.Identity) {
						return std::unexpected{
							git_process_failure::invalid_repository
						};
					}
					const auto initialized = initialize_owned_bare_repository(
						{ .Repository = repository, .Algorithm = aAlgorithm },
						aStop);
					if (!initialized.has_value()) {
						if (
							initialized.error()
							== git_process_failure::invalid_request) {
							continue;
						}
						if (
							initialized.error()
							== git_process_failure::reap_failure) {
							candidateCleanup->retain();
						}
						return std::unexpected{ initialized.error() };
					}
					candidateCleanup->repository_created(*initialized);
					cleanup = std::move(candidateCleanup);
					break;
				}
				if (cleanup == nullptr) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
				std::error_code reapMarkerError;
				if (
					std::filesystem::exists(
						IsolationRoot / "inject-probe-reap-failure",
						reapMarkerError)
					&& !reapMarkerError) {
					cleanup->retain();
					return std::unexpected{
						git_process_failure::reap_failure
					};
				}
#endif
				const auto pack    = probe_pack_identity(aPackHex, aAlgorithm);
				const auto commit  = probe_identity(aCommitHex, aAlgorithm);
				const auto indexed = index_pack(
					{
						.Repository        = repository,
						.Operation         = operation,
						.SpoolBytes        = aPackBytes.size(),
						.MaximumInputBytes = aPackBytes.size(),
						.ExpectedPack      = pack,
					},
					aStop);
				const auto                         inspected    = indexed.has_value()
					? inspect_commit(
						  { .Repository = repository, .Commit = commit },
						  aStop)
					: git_process_result<
						  application::git_commit_inspection>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const auto                         ancestry     = inspected.has_value()
					? is_ancestor(
						  {
							  .Repository = repository,
							  .Ancestor   = commit,
							  .Descendant = commit,
						  },
						  aStop)
					: git_process_result<git_ancestry>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const auto                         connectivity = ancestry.has_value()
					? verify_connectivity(
						  {
							  .Repository = repository,
							  .Base       = commit,
							  .Head       = commit,
						  },
						  aStop)
					: git_process_result<void>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const application::git_staging_ref probeRef{
					.Operation = operation,
					.Endpoint  = application::git_private_ref_endpoint::base,
				};
				const std::array<
					application::git_ref_mutation,
					1U>
						   create{
							   application::git_ref_write{
								   .Ref           = probeRef,
								   .DesiredObject = commit,
								   .ExpectedOld   = application::git_ref_expected_absent{ },
							   }
						   };
				const auto created  = connectivity.has_value()
						&& ancestry == git_ancestry::ancestor
						&& inspected->Tree
							== probe_tree_identity(aTreeHex, aAlgorithm)
					? update_refs(
						  { .Repository = repository, .Mutations = create },
						  aStop)
					: git_process_result<void>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const auto observed = created.has_value()
					? inspect_ref(
						  { .Repository = repository, .Ref = probeRef },
						  aStop)
					: git_process_result<
						  std::optional<git_commit_id>>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const std::array<
					application::git_ref_mutation,
					1U>
						   remove{
							   application::git_ref_remove{
								   .Ref         = probeRef,
								   .ExpectedOld = commit,
							   }
						   };
				const auto removed = observed.has_value() && *observed == commit
					? update_refs(
						  { .Repository = repository, .Mutations = remove },
						  aStop)
					: git_process_result<void>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };
				const auto absent  = removed.has_value()
					? inspect_ref(
						  { .Repository = repository, .Ref = probeRef },
						  aStop)
					: git_process_result<
						  std::optional<git_commit_id>>{
						  std::unexpected{
							  git_process_failure::unsupported_git }
					  };

				const auto reaped =
					[](const auto& aResult) {
						return !aResult.has_value()
							&& aResult.error()
							== git_process_failure::reap_failure;
					};
				if (
					reaped(indexed) || reaped(inspected)
					|| reaped(ancestry) || reaped(connectivity)
					|| reaped(created)
					|| reaped(observed) || reaped(removed)
					|| reaped(absent)) {
					cleanup->retain();
					return std::unexpected{
						git_process_failure::reap_failure
					};
				}
#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
				std::error_code substitutionMarkerError;
				if (
					std::filesystem::exists(
						IsolationRoot
							/ "inject-probe-post-create-substitution",
						substitutionMarkerError)
					&& !substitutionMarkerError) {
					const auto      displacedRepository = RepositoriesRoot / ".displaced-owned-probe.git";
					const auto      displacedSpool      = IncomingRoot / ".displaced-owned-probe.pack.part";
					std::error_code repositoryRenameError;
					std::error_code spoolRenameError;
					std::filesystem::rename(
						repository,
						displacedRepository,
						repositoryRenameError);
					std::filesystem::rename(
						expectedSpool,
						displacedSpool,
						spoolRenameError);
					if (
						repositoryRenameError || spoolRenameError
						|| !make_private_directory(repository)
						|| !write_private_file(
							repository / "replacement-canary",
							"replacement-repository-canary")
						|| !write_private_file(
							expectedSpool,
							"replacement-spool-canary")) {
						cleanup->retain();
						return std::unexpected{
							git_process_failure::invalid_repository
						};
					}
				}
#endif
				const auto cleaned = cleanup->cleanup();
				if (!indexed.has_value()) {
					return std::unexpected{ indexed.error() };
				}
				if (!inspected.has_value()) {
					return std::unexpected{ inspected.error() };
				}
				if (!ancestry.has_value()) {
					return std::unexpected{ ancestry.error() };
				}
				if (!connectivity.has_value()) {
					return std::unexpected{ connectivity.error() };
				}
				if (!created.has_value()) {
					return std::unexpected{ created.error() };
				}
				if (!observed.has_value()) {
					return std::unexpected{ observed.error() };
				}
				if (!removed.has_value()) {
					return std::unexpected{ removed.error() };
				}
				if (!absent.has_value()) {
					return std::unexpected{ absent.error() };
				}
				if (absent->has_value()) {
					return std::unexpected{
						git_process_failure::invalid_request
					};
				}
				if (!cleaned) {
					return std::unexpected{
						git_process_failure::invalid_repository
					};
				}
				return { };
			};

			const auto sha1 = probe(
				git_hash_algorithm::sha1,
				probe_pack,
				"e248b08f09a75116fdb2397c928a84f6a93f6c34",
				"826bb464b84352d60b3d7c56481a599a838d998d",
				"4ce9ffbf7ccd31debb0a343022131f0fde64e35d");
			if (!sha1.has_value()) {
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
			const std::string_view aOutput)
			-> std::optional<std::array<std::uint32_t, 3U>>
		{
			constexpr std::string_view prefix = "git version ";
			if (!aOutput.starts_with(prefix) || aOutput.back() != '\n') {
				return std::nullopt;
			}
			auto                          cursor = prefix.size();
			std::array<std::uint32_t, 3U> version{ };
			for (std::size_t component = 0U; component < version.size(); ++component) {
				if (cursor >= aOutput.size()) {
					return std::nullopt;
				}
				std::uint64_t value = 0U;
				const auto    start = cursor;
				while (
					cursor < aOutput.size()
					&& aOutput[cursor] >= '0' && aOutput[cursor] <= '9') {
					value = value * 10U
						+ static_cast<std::uint64_t>(
								aOutput[cursor] - '0');
					if (value > std::numeric_limits<std::uint32_t>::max()) {
						return std::nullopt;
					}
					++cursor;
				}
				if (cursor == start) {
					return std::nullopt;
				}
				version[component] = static_cast<std::uint32_t>(value);
				if (component + 1U < version.size()) {
					if (cursor >= aOutput.size() || aOutput[cursor] != '.') {
						return std::nullopt;
					}
					++cursor;
				}
			}
			if (cursor >= aOutput.size()) {
				return std::nullopt;
			}
			if (aOutput[cursor] == '\n') {
				return cursor + 1U == aOutput.size()
					? std::optional{ version }
					: std::nullopt;
			}
			const auto suffix = aOutput.substr(
				cursor,
				aOutput.size() - cursor - 1U);
			if (aOutput.back() != '\n') {
				return std::nullopt;
			}
			const auto decimal =
				[](const std::string_view aValue) {
					return !aValue.empty()
						&& std::all_of(
							aValue.begin(),
							aValue.end(),
							[](const char aCharacter) {
								return aCharacter >= '0'
									&& aCharacter <= '9';
							});
				};
			constexpr std::string_view applePrefix = " (Apple Git-";
			if (
				suffix.starts_with(applePrefix)
				&& suffix.ends_with(')')
				&& decimal(suffix.substr(
					applePrefix.size(),
					suffix.size() - applePrefix.size() - 1U))) {
				return version;
			}
			constexpr std::string_view windowsPrefix = ".windows.";
			if (
				suffix.starts_with(windowsPrefix)
				&& decimal(suffix.substr(windowsPrefix.size()))) {
				return version;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto supported_version(
			const std::array<std::uint32_t, 3U>& aVersion) noexcept -> bool
		{
			return aVersion[0] > 2U
				|| (aVersion[0] == 2U && aVersion[1] >= 39U);
		}

		[[nodiscard]] auto prepare_isolation(
			const std::filesystem::path& aIsolation) -> bool
		{
			if (
				!canonical_existing(aIsolation).has_value()
				|| !private_directory(aIsolation)) {
				return false;
			}
			const std::array directories{
				aIsolation / "home",
				aIsolation / "xdg",
				aIsolation / "tmp",
				aIsolation / "work",
				aIsolation / "templates",
				aIsolation / "hooks",
				aIsolation / "incoming",
				aIsolation / "incoming" / "v1",
				aIsolation / "repositories",
				aIsolation / "repositories" / "v1",
			};
			// The isolation's global configuration is the adapter's own — the
			// system one is written too but `GIT_CONFIG_NOSYSTEM` keeps Git
			// from reading it — and on Windows it carries the one setting Git
			// needs there: a ref under `refs/betterpullreview/v1/stage/<sha-256>/`
			// beneath a cache under `%LOCALAPPDATA%` or `%TEMP%` is longer than
			// MAX_PATH, and without `core.longpaths` Git cannot create the
			// directory for its lock file (BPR-0437).
#if defined(Q_OS_WIN)
			constexpr std::string_view globalConfiguration
				= "[core]\n\tlongpaths = true\n";
#else
			constexpr std::string_view globalConfiguration{ };
#endif
			const std::array files{
				std::pair{ aIsolation / "config-system", std::string_view{ } },
				std::pair{ aIsolation / "config-global", globalConfiguration },
				std::pair{ aIsolation / "attributes-system", std::string_view{ } },
			};
			const std::array emptyPolicyDirectories{
				aIsolation / "templates",
				aIsolation / "hooks",
			};

			const auto missing =
				[](const std::filesystem::path& aPath) {
					std::error_code error;
					const auto      status = std::filesystem::symlink_status(aPath, error);
					return (
							   !error
							   && status.type()
								   == std::filesystem::file_type::not_found)
						|| error
						== std::errc::no_such_file_or_directory;
				};
			for (const auto& directory : directories) {
				if (!missing(directory) && !private_directory(directory)) {
					return false;
				}
			}
			for (const auto& [file, contents] : files) {
				if (!missing(file) && !read_exact_file(file, contents)) {
					return false;
				}
			}
			for (const auto& directory : emptyPolicyDirectories) {
				if (!missing(directory) && !empty_private_directory(directory)) {
					return false;
				}
			}

			for (const auto& directory : directories) {
				if (!make_private_directory(directory)) {
					return false;
				}
			}
			for (const auto& [file, contents] : files) {
				if (!write_private_file(file, contents)) {
					return false;
				}
			}
			return std::all_of(
				emptyPolicyDirectories.begin(),
				emptyPolicyDirectories.end(),
				empty_private_directory);
		}

		[[nodiscard]] auto runner_config(
			const std::filesystem::path&                aExecutable,
			const std::filesystem::path&                aIsolation,
			std::optional<std::filesystem::path>        aExecPath,
			const std::optional<std::filesystem::path>& aWindowsSystemRoot)
			-> runner_configuration
		{
			return {
				.Executable        = aExecutable,
				.WorkingDirectory  = aIsolation / "work",
				.Home              = aIsolation / "home",
				.Xdg               = aIsolation / "xdg",
				.Temporary         = aIsolation / "tmp",
				.SystemConfig      = aIsolation / "config-system",
				.GlobalConfig      = aIsolation / "config-global",
				.Attributes        = aIsolation / "attributes-system",
				.Templates         = aIsolation / "templates",
				.Hooks             = aIsolation / "hooks",
				.ExecPath          = std::move(aExecPath),
				.WindowsSystemRoot = aWindowsSystemRoot,
			};
		}
	}

	auto capture_installed_agent_workspace_parent(
		const std::filesystem::path& aPath) noexcept
		-> std::optional<installed_agent_workspace_parent>
	{
		try {
			const auto path = canonical_existing(aPath);
			if (!path.has_value() || !private_directory(*path)) {
				return std::nullopt;
			}
			const auto identity = exact_identity(
				*path,
				exact_artifact_kind::directory);
			if (!identity.has_value()) {
				return std::nullopt;
			}
			return installed_agent_workspace_parent{
				.Path              = *path,
				.DirectoryIdentity = workspace_directory_identity(*identity),
			};
		} catch (...) {
			return std::nullopt;
		}
	}

	auto remove_installed_agent_workspace_parent(
		installed_agent_workspace_parent& aParent) noexcept -> bool
	{
		try {
			const auto identity = exact_directory_identity(
				aParent.DirectoryIdentity);
			if (
				!identity.has_value() || !empty_private_directory(aParent.Path)
				|| exact_identity(aParent.Path, exact_artifact_kind::directory)
					!= identity) {
				return false;
			}
			return quarantine_and_remove_exact(aParent.Path, *identity, false);
		} catch (...) {
			return false;
		}
	}

	auto make_installed_git_process(
		const installed_git_process_configuration& aConfiguration,
		const std::stop_token                      aStop)
		-> std::expected<
			std::unique_ptr<application::git_process_port>,
			application::git_process_error>
	{
		try {
			if (aStop.stop_requested()) {
				return std::unexpected{
					application::git_process_failure::cancelled
				};
			}
			const auto isolation = canonical_existing(aConfiguration.IsolationRoot);
			const auto candidate = canonical_existing(aConfiguration.GitExecutable);
			if (
				!isolation.has_value() || !candidate.has_value()
				|| !private_directory(*isolation)
				|| !regular_executable(*candidate)
				|| !prepare_isolation(*isolation)) {
				return std::unexpected{
					application::git_process_failure::
						invalid_configuration
				};
			}
#if defined(Q_OS_WIN)
			const auto systemRoot = aConfiguration.WindowsSystemRoot.has_value()
				? canonical_existing(*aConfiguration.WindowsSystemRoot)
				: std::nullopt;
			if (!systemRoot.has_value()) {
				return std::unexpected{
					application::git_process_failure::
						invalid_configuration
				};
			}
#else
			const auto systemRoot = std::optional<std::filesystem::path>{ };
#endif

			auto bootstrap = process_worker{
				runner_config(
					*candidate,
					*isolation,
					std::nullopt,
					systemRoot)
			};
			const auto execPathResult = bootstrap.submit(
				process_request{
					.Arguments = { "--exec-path" },
					.Input     = memory_input{ },
					.Timeout   = process_timeout,
				},
				aStop);
			if (!execPathResult.has_value()) {
				return std::unexpected{ std::move(execPathResult.error()) };
			}
			if (
				execPathResult->StandardOutput.empty()
				|| execPathResult->StandardOutput.back() != '\n') {
				return std::unexpected{
					application::git_process_failure::unsupported_git
				};
			}
			const auto execPathText = std::string_view{ execPathResult->StandardOutput }.substr(
				0U,
				execPathResult->StandardOutput.size() - 1U);
			if (
				execPathText.empty()
				|| execPathText.find('\n') != std::string_view::npos
				|| execPathText.find('\r') != std::string_view::npos) {
				return std::unexpected{
					application::git_process_failure::standard_output_limit
				};
			}
			const auto execPath = canonical_existing(
				path_from_text(execPathText));
			if (!execPath.has_value()) {
				return std::unexpected{
					application::git_process_failure::invalid_configuration
				};
			}
			const auto resolvedGit = resolve_canonical_existing(
				*execPath
#if defined(Q_OS_WIN)
				/ "git.exe"
#else
				/ "git"
#endif
			);
			if (
				!resolvedGit.has_value()
				|| !regular_executable(*resolvedGit)) {
				return std::unexpected{
					application::git_process_failure::unsupported_git
				};
			}

			auto worker = std::make_unique<process_worker>(
				runner_config(
					*resolvedGit,
					*isolation,
					execPath,
					systemRoot));
			const auto matchingExecPath = worker->submit(
				process_request{
					.Arguments = { "--exec-path" },
					.Input     = memory_input{ },
					.Timeout   = process_timeout,
				},
				aStop);
			const auto versionResult = worker->submit(
				process_request{
					.Arguments = { "--version" },
					.Input     = memory_input{ },
					.Timeout   = process_timeout,
				},
				aStop);
			const auto expectedExecPath = path_text(*execPath) + "\n";
			if (!matchingExecPath.has_value()) {
				return std::unexpected{ std::move(matchingExecPath.error()) };
			}
			if (!versionResult.has_value()) {
				return std::unexpected{ std::move(versionResult.error()) };
			}
			if (
				matchingExecPath->StandardOutput
				!= expectedExecPath) {
				return std::unexpected{
					application::git_process_failure::unsupported_git
				};
			}
			const auto version = parse_version(versionResult->StandardOutput);
			if (!version.has_value() || !supported_version(*version)) {
				return std::unexpected{
					application::git_process_failure::unsupported_git
				};
			}

			auto implementation = std::make_unique<installed_git_process>(
				application::git_installation{
					.VersionMajor = (*version)[0],
					.VersionMinor = (*version)[1],
					.VersionPatch = (*version)[2],
				},
				*isolation,
				std::move(worker));
			const auto probe = implementation->capability_probe(aStop);
			if (!probe.has_value()) {
				if (
					probe.error()
					== application::git_process_failure::reap_failure) {
					// The worker may still own a live child and its probe files.
					// Keep the complete boundary quarantined instead of allowing
					// either its destructor or probe cleanup to race that child.
					static_cast<void>(implementation.release());
					return std::unexpected{ std::move(probe.error()) };
				}
				if (
					probe.error()
					== application::git_process_failure::cancelled) {
					return std::unexpected{ std::move(probe.error()) };
				}
				return std::unexpected{ application::git_process_error{
					application::git_process_failure::unsupported_git,
					std::move(probe.error().Detail) } };
			}
			return std::unique_ptr<application::git_process_port>{
				std::move(implementation)
			};
		} catch (...) {
			return std::unexpected{
				application::git_process_failure::invalid_configuration
			};
		}
	}
}
