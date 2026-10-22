#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/process_support/process_supervision.hpp"
#include "bpr/platform/windows_sharing_retry.hpp"
#include "git_process_output.hpp"
#include <glaze/json.hpp>

#include <QByteArray>
#include <QCoreApplication>
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
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
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
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>

	#include "bpr/platform/unix_descriptor_sweep.hpp"
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

	#include "bpr/adapters/agent/windows_workspace_guard.hpp"
	#include "bpr/platform/windows_standard_handles.hpp"
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
		using clock = process_support::supervision_clock;
		using process_support::kill_process_group;
		using process_support::process_stream;
		using process_support::resolve_process_group;
		using process_support::wait_milliseconds;
		using process_support::wait_slice;

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

		// Turns a non-zero exit's retained standard output into fixed,
		// secret-free text for the failure detail (BPR-0852). Null keeps
		// standard output out of the detail.
		using failed_output_summarizer = std::string (*)(std::string_view);

		struct process_request
		{
			std::vector<std::string>                                   Arguments;
			process_input                                              Input                 = memory_input{ };
			std::chrono::milliseconds                                  Timeout               = process_timeout;
			std::size_t                                                StandardOutputLimit   = standard_output_limit;
			bool                                                       AllowExitOne          = false;
			failed_output_summarizer                                   SummarizeFailedOutput = nullptr;
			std::optional<application::agent_workspace_lifetime_guard> WorkspaceGuard;
			/// Variables added to the fixed environment for this one command.
			/// Only the publication catalog sets any, from a closed list.
			std::vector<std::pair<std::string, std::string>> Environment;
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

		/// \brief The objects folder a checkout's Git directory uses, followed
		/// through a `.git` file and `commondir` as Git follows them, or nothing.
		/// \details One path that the alternates list can carry as a single
		/// entry: no separator, quote or line break. Shared by publication,
		/// which reads a chosen checkout's objects, and by an agent workspace
		/// that borrows a linked clone's history ([[BPR-PENDING-AQ4]]).
		[[nodiscard]] auto checkout_objects(const std::string& aCheckout) -> std::optional<std::filesystem::path>
		{
			const auto root = resolve_canonical_existing(path_from_text(aCheckout));
			if (!root) {
				return std::nullopt;
			}
			const auto smallText = [](const std::filesystem::path& aPath) -> std::optional<std::string> {
				std::error_code error;
				if (!std::filesystem::is_regular_file(aPath, error) || error || std::filesystem::file_size(aPath, error) > 4096U || error) {
					return std::nullopt;
				}
				std::ifstream input{ aPath, std::ios::binary };
				auto          text = std::string{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
				while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
					text.pop_back();
				}
				return input.bad() ? std::nullopt : std::optional{ std::move(text) };
			};
			const auto relative = [](const std::filesystem::path& aBase, const std::string& aText) {
				const auto path = path_from_text(aText);
				return path.is_absolute() ? path : aBase / path;
			};
			std::error_code error;
			auto            gitDirectory = *root / ".git";
			if (std::filesystem::is_regular_file(gitDirectory, error) && !error) {
				const auto pointer = smallText(gitDirectory);
				if (!pointer || !pointer->starts_with("gitdir: ")) {
					return std::nullopt;
				}
				gitDirectory = relative(*root, pointer->substr(8U));
			}
			auto common = resolve_canonical_existing(gitDirectory);
			if (!common) {
				return std::nullopt;
			}
			if (const auto shared = smallText(*common / "commondir")) {
				common = resolve_canonical_existing(relative(*common, *shared));
				if (!common) {
					return std::nullopt;
				}
			}
			auto objects = resolve_canonical_existing(*common / "objects");
			if (!objects || !std::filesystem::is_directory(*objects, error) || error) {
				return std::nullopt;
			}
			// The alternates list is separator-delimited; a quoted or
			// multi-entry form is never produced.
			const auto text = path_text(*objects);
#if defined(Q_OS_WIN)
			constexpr auto separator = ';';
#else
			constexpr auto separator = ':';
#endif
			if (text.find(separator) != std::string::npos || text.find_first_of("\"\n\r") != std::string::npos) {
				return std::nullopt;
			}
			return objects;
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
				&& (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0;
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
					&& (information.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0
					&& information.st_size >= 0
					&& static_cast<std::uint64_t>(information.st_size)
					== aExpectedBytes;
#elif defined(Q_OS_WIN)
				// [[BPR-1040]]. Opened with no sharing, so a scanner still reading
				// the spool this adapter just wrote refuses it for a moment.
				const auto opened = platform::retry_while_held(platform::held_errors::sharing_lock_and_access_denied, [&]() -> DWORD {
					Handle = CreateFileW(
						aPath.c_str(),
						GENERIC_READ,
						0U,
						nullptr,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
							| FILE_FLAG_SEQUENTIAL_SCAN,
						nullptr);
					return Handle != INVALID_HANDLE_VALUE ? ERROR_SUCCESS : GetLastError();
				});
				if (opened != ERROR_SUCCESS) {
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
				static_cast<void>(aDestination);
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
			static_cast<void>(aPath);
			static_cast<void>(aKind);
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
					|| (before.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) != 0)) {
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
			const auto      status = std::filesystem::symlink_status(aPath, error);
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
					aPath,
					std::ios::binary | std::ios::trunc
				};
				output.write(
					reinterpret_cast<const char*>(aBytes.data()),
					static_cast<std::streamsize>(aBytes.size()));
				output.flush();
				output.close();
				if (!output) {
					return false;
				}
			} catch (...) {
				return false;
			}
			std::filesystem::permissions(
				aPath,
				std::filesystem::perms::owner_read
					| std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace,
				error);
			return !error && private_regular_file(aPath);
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
				&& (opened.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0;
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
				aPath,
				std::ios::binary | std::ios::out | std::ios::noreplace
			};
			if (!output) {
				return { };
			}
			output.write(
				reinterpret_cast<const char*>(aBytes.data()),
				static_cast<std::streamsize>(aBytes.size()));
			output.flush();
			output.close();
			std::error_code error;
			std::filesystem::permissions(
				aPath,
				std::filesystem::perms::owner_read
					| std::filesystem::perms::owner_write,
				std::filesystem::perm_options::replace,
				error);
			return {
				.Created  = true,
				.Complete = !error && output && private_regular_file(aPath),
				.Identity = exact_identity(
					aPath,
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

		// [[BPR-0970]]. The output readers live in git_process_output.hpp, where
		// the parser fuzz target reaches them.
		using detail::hex_value;
		using detail::parse_object;
		using detail::tree_entry_path;
		using detail::tree_entry_path_text;
		using detail::valid_tree_entry_path;

		template <typename Object>
		[[nodiscard]] auto object_text(const Object& aObject) -> std::string
		{
			const auto byteCount = aObject.Algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
			return lower_hex(
				std::span{ aObject.Digest }.first(byteCount));
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

			/// \brief Creates the kill-on-close job, before the launch.
			///
			/// \details [[BPR-1265]]. The launch names this job in its creation
			/// list (`launch_restriction::place_in_job`), so process creation
			/// puts the child in it. Assigning the started child by its process
			/// id instead raced it: a Git command that had already finished
			/// could no longer be assigned (`ERROR_ACCESS_DENIED`), and the
			/// request failed as `process_start` although Git had succeeded.
			[[nodiscard]] auto create() noexcept -> bool
			{
#if defined(Q_OS_WIN)
				Handle = CreateJobObjectW(nullptr, nullptr);
				if (Handle == nullptr) {
					return false;
				}
				JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{ };
				limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				return SetInformationJobObject(
						   Handle,
						   JobObjectExtendedLimitInformation,
						   &limits,
						   sizeof(limits))
					!= 0;
#else
				return true;
#endif
			}

#if defined(Q_OS_WIN)
			[[nodiscard]] auto native() const noexcept -> HANDLE
			{
				return Handle;
			}
#endif

			/// \brief Whether the started child is in this job.
			///
			/// \details Process creation added it or created nothing, so this
			/// is a check, not an assignment. Qt still holds the child's handle,
			/// so its process id names this child even after it has exited. A
			/// child found outside the job, or one that cannot be checked, is
			/// terminated and refused; one that has finished inside it is not
			/// a failure.
			[[nodiscard]] auto holds(const qint64 aProcessId) noexcept -> bool
			{
#if defined(Q_OS_WIN)
				if (Handle == nullptr) {
					return false;
				}
				const auto process = OpenProcess(
					PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
					FALSE,
					static_cast<DWORD>(aProcessId));
				if (process == nullptr) {
					return false;
				}
				auto       inside  = FALSE;
				const auto checked = IsProcessInJob(process, Handle, &inside) != 0;
				if (!checked || inside == FALSE) {
					static_cast<void>(TerminateProcess(process, 1U));
				}
				static_cast<void>(CloseHandle(process));
				return checked && inside != FALSE;
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
		/// bounds each append by the installed-Git standard-error limit, including
		/// replacements that expand the original token.
		[[nodiscard]] auto redacted_diagnostic(
			const std::string_view aText) -> std::string
		{
			std::string result;
			result.reserve(std::min(aText.size(), standard_error_limit));
			const auto append = [&result](const std::string_view aPart) {
				result.append(aPart.substr(0U, standard_error_limit - result.size()));
			};
			std::size_t index = 0U;
			while (index < aText.size() && result.size() < standard_error_limit) {
				const auto tokenStart = aText.find_first_not_of(
					diagnostic_whitespace,
					index);
				append(
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
				append(
					remote_url_token(token)
						? redacted_url_placeholder
						: token);
				index = tokenEnd;
			}
			return result;
		}

		/// \brief Counts what `git fsck --connectivity-only` found, without repeating it.
		/// \details On a broken closure fsck writes `broken link` and `missing <type> <oid>`
		/// lines to standard output, exits non-zero and may leave standard error empty
		/// (BPR-0852). The summary keeps fixed words and counts only: never an object
		/// name, a path or an unrecognised line's text. Its input is the runner's
		/// retained output, which the standard-output limit already bounds.
		[[nodiscard]] auto summarize_connectivity_output(
			const std::string_view aOutput) -> std::string
		{
			static constexpr std::array<std::string_view, 4U> kinds{ "commit", "tree", "blob", "tag" };
			auto                                              missing = std::array<std::size_t, 4U>{ };
			std::size_t                                       broken  = 0U;
			std::size_t                                       other   = 0U;
			std::size_t                                       start   = 0U;
			while (start < aOutput.size()) {
				const auto end   = std::min(aOutput.find('\n', start), aOutput.size());
				const auto line  = aOutput.substr(start, end - start);
				start            = end + 1U;
				const auto first = line.find_first_not_of(' ');
				if (first == std::string_view::npos) {
					continue;
				}
				const auto text = line.substr(first);
				if (text.starts_with("broken link from ")) {
					++broken;
					continue;
				}
				// The second line of a `broken link` report is indented.
				if (first != 0U && text.starts_with("to ")) {
					continue;
				}
				if (first == 0U && text.starts_with("missing ")) {
					const auto rest  = text.substr(8U);
					const auto type  = rest.substr(0U, rest.find(' '));
					const auto found = std::ranges::find(kinds, type);
					if (found != kinds.end() && type.size() != rest.size()) {
						++missing[static_cast<std::size_t>(found - kinds.begin())];
						continue;
					}
				}
				++other;
			}
			std::string summary;
			const auto  append = [&](const std::size_t aCount, const std::string_view aNoun) {
				if (aCount == 0U) {
					return;
				}
				summary += summary.empty() ? "fsck reported " : ", ";
				summary += std::to_string(aCount);
				summary += ' ';
				summary += aNoun;
				if (aCount != 1U) {
					summary += 's';
				}
			};
			for (std::size_t index = 0U; index < kinds.size(); ++index) {
				append(missing[index], "missing " + std::string{ kinds[index] });
			}
			append(broken, "broken link");
			append(other, "other line");
			return summary;
		}

		/// \brief Appends a fixed output summary to a redacted diagnostic within the stderr limit.
		[[nodiscard]] auto with_output_summary(
			std::string            aDiagnostic,
			const std::string_view aSummary) -> std::string
		{
			if (aSummary.empty() || aSummary.size() >= standard_error_limit) {
				return aDiagnostic;
			}
			if (aDiagnostic.size() > standard_error_limit - aSummary.size() - 1U) {
				aDiagnostic.resize(standard_error_limit - aSummary.size() - 1U);
			}
			if (!aDiagnostic.empty() && aDiagnostic.back() != '\n') {
				aDiagnostic.push_back('\n');
			}
			aDiagnostic.append(aSummary);
			return aDiagnostic;
		}

		/// \brief Git's own default severity for one fsck message identifier.
		enum class fsck_severity : std::uint8_t {
			fatal,
			error,
			warn,
			info,
			ignored,
		};

		/// \brief One entry of Git's fsck message table, as this adapter reads it.
		///
		/// \details `Since*` is the oldest Git measured to recognise the
		/// identifier, never this machine's version. Git answers an identifier
		/// it does not know with `fatal: Unhandled message id` and exits 128, so
		/// shipping one an installation has never heard of would fail every
		/// import on it. An identifier is therefore only ever offered to an
		/// installation at or above its recorded version, and a version recorded
		/// too high only leaves that Git as strict as it is today. BPR-0880
		/// measured every row against Git 2.34.1, Git 2.39.5 — the oldest
		/// admitted Git that could be obtained — and Git 2.53.0. A row recorded
		/// at 2.39.0 is one the 2.34.1 probe already knew, and Git does not
		/// retire message identifiers, so the admitted floor knows it too.
		struct fsck_message final
		{
			std::string_view Id;
			fsck_severity    Severity;
			std::uint32_t    SinceMajor;
			std::uint32_t    SinceMinor;
			std::uint32_t    SincePatch;
		};

		/// \brief Whether the installation is new enough to know an identifier.
		[[nodiscard]] auto knows_fsck_message(
			const application::git_installation& aInstallation,
			const fsck_message&                  aMessage) -> bool
		{
			if (aInstallation.VersionMajor != aMessage.SinceMajor) {
				return aInstallation.VersionMajor > aMessage.SinceMajor;
			}
			if (aInstallation.VersionMinor != aMessage.SinceMinor) {
				return aInstallation.VersionMinor > aMessage.SinceMinor;
			}
			return aInstallation.VersionPatch >= aMessage.SincePatch;
		}

		/// \brief Git's object-level fsck messages with their default severities.
		///
		/// \details Read from Git 2.53.0's own `git fsck` message table and
		/// confirmed by running `index-pack` against objects that trigger them
		/// (BPR-0880). Ref-storage identifiers (`badRefContent`, `symlinkRef`,
		/// the `packed-refs` and reftable families) are absent on purpose:
		/// `index-pack` inspects objects and can never report them, so naming
		/// them would only add identifiers an older Git might not know.
		///
		/// The `warn` and `info` rows the installation is old enough to know are
		/// the closed list this adapter passes to Git. `git clone` accepts every
		/// one of them, Git's own transfer path leaves `fetch.fsckObjects` false,
		/// and this cache verifies pack closure separately with
		/// `verify_connectivity`, so refusing a repository Git itself accepts
		/// costs the reviewer the whole pull request for integrity the product
		/// does not rely on. `=ignore` rather than `=warn` because `=warn` still
		/// writes the finding to standard error and an accepted exit that wrote
		/// to standard error is malformed output here, which would leave the
		/// repository just as unopenable while quietly weakening BPR-0846's
		/// exact-output rule. `info` is in the list for that second reason
		/// alone: it never made `index-pack` exit nonzero, but the `warning:`
		/// line it wrote reached the same dead end.
		///
		/// `error` rows stay fatal, and the two `fatal` rows cannot be demoted
		/// at all — Git answers `fatal: Cannot demote ... to ignore`. Both kinds
		/// are listed anyway so that a refusal can be named rather than reported
		/// as a bare process failure.
		// BPR_0880_FSCK_MESSAGE_TABLE_BEGIN
		inline constexpr std::array<fsck_message, 60U> fsck_messages{ {
			{ "badDate", fsck_severity::error, 2U, 39U, 0U },
			{ "badDateOverflow", fsck_severity::error, 2U, 39U, 0U },
			{ "badEmail", fsck_severity::error, 2U, 39U, 0U },
			{ "badFilemode", fsck_severity::info, 2U, 39U, 0U },
			{ "badGpgsig", fsck_severity::error, 2U, 53U, 0U },
			{ "badHeaderContinuation", fsck_severity::error, 2U, 53U, 0U },
			{ "badName", fsck_severity::error, 2U, 39U, 0U },
			{ "badObjectSha1", fsck_severity::error, 2U, 39U, 0U },
			{ "badParentSha1", fsck_severity::error, 2U, 39U, 0U },
			{ "badTagName", fsck_severity::info, 2U, 39U, 0U },
			{ "badTimezone", fsck_severity::error, 2U, 39U, 0U },
			{ "badTree", fsck_severity::error, 2U, 39U, 0U },
			{ "badTreeSha1", fsck_severity::error, 2U, 39U, 0U },
			{ "badType", fsck_severity::error, 2U, 39U, 0U },
			{ "duplicateEntries", fsck_severity::error, 2U, 39U, 0U },
			{ "emptyName", fsck_severity::warn, 2U, 39U, 0U },
			{ "extraHeaderEntry", fsck_severity::ignored, 2U, 39U, 0U },
			{ "fullPathname", fsck_severity::warn, 2U, 39U, 0U },
			{ "gitattributesBlob", fsck_severity::error, 2U, 39U, 5U },
			{ "gitattributesLarge", fsck_severity::error, 2U, 39U, 5U },
			{ "gitattributesLineLength", fsck_severity::error, 2U, 39U, 5U },
			{ "gitattributesMissing", fsck_severity::error, 2U, 39U, 5U },
			{ "gitattributesSymlink", fsck_severity::info, 2U, 39U, 0U },
			{ "gitignoreSymlink", fsck_severity::info, 2U, 39U, 0U },
			{ "gitmodulesBlob", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesLarge", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesMissing", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesName", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesParse", fsck_severity::info, 2U, 39U, 0U },
			{ "gitmodulesPath", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesSymlink", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesUpdate", fsck_severity::error, 2U, 39U, 0U },
			{ "gitmodulesUrl", fsck_severity::error, 2U, 39U, 0U },
			{ "hasDot", fsck_severity::warn, 2U, 39U, 0U },
			{ "hasDotdot", fsck_severity::warn, 2U, 39U, 0U },
			{ "hasDotgit", fsck_severity::warn, 2U, 39U, 0U },
			{ "largePathname", fsck_severity::warn, 2U, 53U, 0U },
			{ "mailmapSymlink", fsck_severity::info, 2U, 39U, 0U },
			{ "missingAuthor", fsck_severity::error, 2U, 39U, 0U },
			{ "missingCommitter", fsck_severity::error, 2U, 39U, 0U },
			{ "missingEmail", fsck_severity::error, 2U, 39U, 0U },
			{ "missingNameBeforeEmail", fsck_severity::error, 2U, 39U, 0U },
			{ "missingObject", fsck_severity::error, 2U, 39U, 0U },
			{ "missingSpaceBeforeDate", fsck_severity::error, 2U, 39U, 0U },
			{ "missingSpaceBeforeEmail", fsck_severity::error, 2U, 39U, 0U },
			{ "missingTag", fsck_severity::error, 2U, 39U, 0U },
			{ "missingTagEntry", fsck_severity::error, 2U, 39U, 0U },
			{ "missingTaggerEntry", fsck_severity::info, 2U, 39U, 0U },
			{ "missingTree", fsck_severity::error, 2U, 39U, 0U },
			{ "missingType", fsck_severity::error, 2U, 39U, 0U },
			{ "missingTypeEntry", fsck_severity::error, 2U, 39U, 0U },
			{ "multipleAuthors", fsck_severity::error, 2U, 39U, 0U },
			{ "nulInCommit", fsck_severity::warn, 2U, 39U, 0U },
			{ "nulInHeader", fsck_severity::fatal, 2U, 39U, 0U },
			{ "nullSha1", fsck_severity::warn, 2U, 39U, 0U },
			{ "treeNotSorted", fsck_severity::error, 2U, 39U, 0U },
			{ "unknownType", fsck_severity::error, 2U, 39U, 0U },
			{ "unterminatedHeader", fsck_severity::fatal, 2U, 39U, 0U },
			{ "zeroPaddedDate", fsck_severity::error, 2U, 39U, 0U },
			{ "zeroPaddedFilemode", fsck_severity::warn, 2U, 39U, 0U },
		} };
		// BPR_0880_FSCK_MESSAGE_TABLE_END

		/// \brief Builds the closed `<msg-id>=ignore` list Git is given.
		[[nodiscard]] auto ignored_fsck_message_policy(
			const application::git_installation& aInstallation) -> std::string
		{
			std::string policy;
			for (const auto& message : fsck_messages) {
				if (
					(message.Severity != fsck_severity::warn
					 && message.Severity != fsck_severity::info)
					|| !knows_fsck_message(aInstallation, message)) {
					continue;
				}
				if (!policy.empty()) {
					policy.push_back(',');
				}
				policy.append(message.Id);
				policy.append("=ignore");
			}
			return policy;
		}

		/// \brief Names the fsck message class an `index-pack` refusal reported.
		///
		/// \details Git writes one `error: object <oid>: <msg-id>: <text>` line
		/// per refused object. Only the identifier is taken, and only when the
		/// table above already contains it, so the returned text is always one of
		/// this adapter's own fixed names and never a path, an object name, or
		/// anything else the pack's author chose (BPR-0852's rule). An
		/// unrecognized refusal names nothing and stays a plain nonzero exit.
		[[nodiscard]] auto refused_fsck_message(
			const std::string_view aDiagnostic) -> std::string_view
		{
			constexpr std::string_view reported{ "error: object " };
			for (std::size_t offset = 0U; offset < aDiagnostic.size();) {
				const auto ending = aDiagnostic.find('\n', offset);
				const auto line   = aDiagnostic.substr(
					offset,
					ending == std::string_view::npos ? ending : ending - offset);
				offset = ending == std::string_view::npos
					? aDiagnostic.size()
					: ending + 1U;
				if (!line.starts_with(reported)) {
					continue;
				}
				const auto named  = line.substr(reported.size());
				const auto object = named.find(": ");
				if (object == std::string_view::npos) {
					continue;
				}
				const auto identifier = named.substr(object + 2U);
				const auto terminator = identifier.find(':');
				if (terminator == std::string_view::npos) {
					continue;
				}
				const auto candidate = identifier.substr(0U, terminator);
				const auto known     = std::ranges::find(
					fsck_messages,
					candidate,
					&fsck_message::Id);
				if (known != fsck_messages.end()) {
					return known->Id;
				}
			}
			return { };
		}

#if defined(Q_OS_WIN)
		using launch_restriction = bpr::platform::windows_standard_handles::launch_restriction;

		/// \brief Starts an unguarded Git with exactly its three standard handles.
		///
		/// \details [[BPR-1104]], [[BPR-1115]]. The platform target's launch
		/// restriction: the Windows counterpart of the `CloseFileDescriptors`
		/// flag every Unix launch sets, and the rule the supervisor already
		/// applies to a guarded Git. Qt creates the child with handle inheritance
		/// on and no list, so without one Git inherited every inheritable handle
		/// this process held at that instant, and kept it until it exited. On the
		/// Windows station a C runtime stream reading one epoch's pack spool was
		/// inherited by the `index-pack` of another, and the spool's exclusive
		/// open and its removal were refused (`ERROR_SHARING_VIOLATION`) for as
		/// long as that Git ran.
		///
		/// A launch whose list cannot be built is refused by process creation
		/// itself: no Git is created, and the start fails as `process_start`.
		/// \p aInheritance must outlive the start.
		void restrict_to_standard_handles(
			QProcess&                                     aProcess,
			launch_restriction&                           aInheritance,
			[[maybe_unused]] const std::filesystem::path& aIsolation)
		{
	#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
			// The contract's seam for the refusal: withholding the standard error
			// handle, as a null standard handle would, leaves the list unbuildable.
			std::error_code markerError;
			if (std::filesystem::exists(aIsolation / "inject-standard-handle-list-failure", markerError) && !markerError) {
				aProcess.setCreateProcessArgumentsModifier([&aInheritance](QProcess::CreateProcessArguments* const aArguments) {
					aArguments->startupInfo->hStdError = nullptr;
					aInheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles);
				});
				return;
			}
	#endif
			aProcess.setCreateProcessArgumentsModifier([&aInheritance](QProcess::CreateProcessArguments* const aArguments) {
				aInheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles);
			});
		}
#endif

		// The supervisor, rather than Git, retains the journal lock. Git may
		// close inherited descriptors or leave descendants behind when it exits.
		struct git_workspace_lifetime
		{
			enum class start_state : std::uint8_t {
				pending,
				started,
				failed,
			};

			bool Supervised = false;
#if defined(Q_OS_UNIX)
			std::array<int, 2> Channel{ -1, -1 };
			int                Guard = -1;
			static void        close_descriptor(int& aDescriptor) noexcept
			{
				if (aDescriptor >= 0) {
					static_cast<void>(::close(aDescriptor));
					aDescriptor = -1;
				}
			}
#elif defined(Q_OS_WIN)
			// [[BPR-0622]]. A LockFileEx lock belongs to the process that took it,
			// so the bundled supervisor takes its own before it creates Git; this
			// process keeps a shared lock only for this execution. The supervisor
			// creates Git suspended inside its own kill-on-close job and reports
			// before Git runs, so no Git write precedes job assignment.
			static constexpr DWORD                  lifetime_pipe_bytes = 256U * 1024U;
			adapters::agent::detail::windows_handle Guard;
			adapters::agent::detail::windows_handle LifetimeOwner;
			adapters::agent::detail::windows_handle LifetimeChild;
			adapters::agent::detail::windows_handle ReportOwner;
			adapters::agent::detail::windows_handle ReportChild;
			std::wstring                            GuardPath;
			std::array<std::uint64_t, 4U>           GuardIdentity{ };
			std::wstring                            SupervisorPath;
			std::wstring                            SupervisorCommand;
			std::wstring                            GitExecutable;
			std::wstring                            GitCommand;
			std::wstring                            GitWorkingDirectory;
			std::wstring                            SupervisorDirectory;
			std::vector<BYTE>                       AttributeBuffer;
			LPPROC_THREAD_ATTRIBUTE_LIST            Attributes = nullptr;
			std::vector<HANDLE>                     Inherited;
			STARTUPINFOEXW                          Startup{ };
			bool                                    Redirected           = false;
			bool                                    SpecificationWritten = false;
			start_state                             Start                = start_state::pending;

			[[nodiscard]] static auto make_pipe(
				adapters::agent::detail::windows_handle& aOwner,
				adapters::agent::detail::windows_handle& aChild,
				const bool                               aOwnerWrites,
				const DWORD                              aBytes) noexcept -> bool
			{
				auto attributes           = SECURITY_ATTRIBUTES{ };
				attributes.nLength        = sizeof(attributes);
				attributes.bInheritHandle = TRUE;
				auto read                 = HANDLE{ nullptr };
				auto write                = HANDLE{ nullptr };
				if (::CreatePipe(&read, &write, &attributes, aBytes) == FALSE) {
					return false;
				}
				aOwner.reset(aOwnerWrites ? write : read);
				aChild.reset(aOwnerWrites ? read : write);
				return ::SetHandleInformation(aOwner.get(), HANDLE_FLAG_INHERIT, 0U) != FALSE;
			}

			[[nodiscard]] auto write_value(const void* const aData, const DWORD aBytes) const noexcept -> bool
			{
				auto written = DWORD{ 0U };
				while (written < aBytes) {
					auto count = DWORD{ 0U };
					if (::WriteFile(LifetimeOwner.get(), static_cast<const char*>(aData) + written, aBytes - written, &count, nullptr) == FALSE || count == 0U) {
						return false;
					}
					written += count;
				}
				return true;
			}

			[[nodiscard]] auto write_string(const std::wstring& aValue) const noexcept -> bool
			{
				const auto length = static_cast<DWORD>(aValue.size());
				return !aValue.empty() && aValue.size() <= 32'767U
					&& write_value(&length, sizeof(length))
					&& write_value(aValue.data(), length * static_cast<DWORD>(sizeof(wchar_t)));
			}

			/// Replaces the Git launch Qt prepared with the supervisor's, keeping
			/// Git's exact command line for the specification. A failure still
			/// starts the supervisor, with arguments it refuses, so Git never runs
			/// outside it.
			void redirect(QProcess::CreateProcessArguments* const aArguments) noexcept
			{
				try {
					Supervised           = true;
					GitCommand           = aArguments->arguments != nullptr ? std::wstring{ aArguments->arguments } : std::wstring{ };
					GitWorkingDirectory  = aArguments->currentDirectory != nullptr ? std::wstring{ aArguments->currentDirectory } : std::wstring{ };
					const auto& original = *aArguments->startupInfo;
					Inherited.clear();
					for (const auto handle : { original.hStdInput, original.hStdOutput, original.hStdError, LifetimeChild.get(), ReportChild.get() }) {
						if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
							throw std::runtime_error{ "supervisor handle" };
						}
						if (std::ranges::find(Inherited, handle) == Inherited.end()) {
							Inherited.push_back(handle);
						}
					}
					auto size = SIZE_T{ 0U };
					static_cast<void>(::InitializeProcThreadAttributeList(nullptr, 1U, 0U, &size));
					AttributeBuffer.assign(size, BYTE{ 0U });
					const auto list = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(static_cast<void*>(AttributeBuffer.data()));
					if (size == 0U || ::InitializeProcThreadAttributeList(list, 1U, 0U, &size) == FALSE) {
						throw std::runtime_error{ "supervisor attributes" };
					}
					Attributes = list;
					if (GitCommand.empty() || GitWorkingDirectory.empty()
						|| ::UpdateProcThreadAttribute(Attributes, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, Inherited.data(), Inherited.size() * sizeof(HANDLE), nullptr, nullptr) == FALSE) {
						throw std::runtime_error{ "supervisor handle list" };
					}
					Startup                 = STARTUPINFOEXW{ };
					Startup.StartupInfo     = original;
					Startup.StartupInfo.cb  = sizeof(Startup);
					Startup.lpAttributeList = Attributes;
					aArguments->startupInfo = &Startup.StartupInfo;
					aArguments->flags |= EXTENDED_STARTUPINFO_PRESENT;
					aArguments->inheritHandles   = true;
					aArguments->applicationName  = SupervisorPath.c_str();
					aArguments->arguments        = SupervisorCommand.data();
					aArguments->currentDirectory = SupervisorDirectory.c_str();
					Redirected                   = true;
				} catch (...) {
					// The supervisor with no owner arguments exits at once.
					SupervisorCommand           = L"\"" + SupervisorPath + L"\"";
					aArguments->applicationName = SupervisorPath.c_str();
					aArguments->arguments       = SupervisorCommand.data();
					aArguments->inheritHandles  = false;
					Redirected                  = false;
				}
			}
#endif
			void close_owner() noexcept
			{
#if defined(Q_OS_UNIX)
				close_descriptor(Channel[0]);
#elif defined(Q_OS_WIN)
				LifetimeOwner.reset();
#endif
			}
			void close_child() noexcept
			{
#if defined(Q_OS_UNIX)
				close_descriptor(Channel[1]);
				close_descriptor(Guard);
#elif defined(Q_OS_WIN)
				LifetimeChild.reset();
				ReportChild.reset();
#endif
			}
			~git_workspace_lifetime()
			{
				close_owner();
				close_child();
#if defined(Q_OS_WIN)
				if (Attributes != nullptr) {
					::DeleteProcThreadAttributeList(Attributes);
				}
#endif
			}

			/// Unix learns of a failed start as the supervisor's exit. Windows
			/// sends the launch specification and waits for the supervisor's
			/// report that Git is assigned to its job and resumed.
			[[nodiscard]] auto poll_start() noexcept -> start_state
			{
#if defined(Q_OS_WIN)
				if (Start != start_state::pending) {
					return Start;
				}
				if (!Redirected) {
					return Start = start_state::failed;
				}
				if (!SpecificationWritten) {
					SpecificationWritten       = true;
					const auto magic           = DWORD{ 0x53525042U };
					const auto version         = DWORD{ 2U };
					const auto flags           = DWORD{ 1U };
					const auto specificationOk = write_value(&magic, sizeof(magic))
						&& write_value(&version, sizeof(version))
						&& write_value(&flags, sizeof(flags))
						&& write_string(GitExecutable)
						&& write_string(GitCommand)
						&& write_string(GitWorkingDirectory)
						&& write_string(GuardPath)
						&& write_value(GuardIdentity.data(), static_cast<DWORD>(sizeof(GuardIdentity)));
					if (!specificationOk) {
						return Start = start_state::failed;
					}
				}
				auto available = DWORD{ 0U };
				if (::PeekNamedPipe(ReportOwner.get(), nullptr, 0U, nullptr, &available, nullptr) == FALSE) {
					return Start = start_state::failed;
				}
				if (available < sizeof(std::int32_t)) {
					return start_state::pending;
				}
				auto report = std::int32_t{ -1 };
				auto count  = DWORD{ 0U };
				if (::ReadFile(ReportOwner.get(), &report, sizeof(report), &count, nullptr) == FALSE || count != sizeof(report)) {
					return Start = start_state::failed;
				}
				return Start = report == 0 ? start_state::started : start_state::failed;
#else
				return start_state::started;
#endif
			}
			[[nodiscard]] auto prepare(QProcess&                                          aProcess,
									   const application::agent_workspace_lifetime_guard& aGuard) -> bool
			{
#if defined(Q_OS_UNIX)
				const auto path = std::filesystem::path{ aGuard.Path };
				if (aGuard.Path.empty() || aGuard.Path.size() > 4096U
					|| aGuard.Path.find('\0') != std::string::npos
					|| !path.is_absolute() || canonical_existing(path) != path) {
					return false;
				}
				const auto opened = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
				if (opened < 0) {
					return false;
				}
				Guard = ::fcntl(opened, F_DUPFD_CLOEXEC, 5);
				static_cast<void>(::close(opened));
				struct stat original{ }, named{ };
				if (Guard < 0 || ::fstat(Guard, &original) != 0
					|| !S_ISREG(original.st_mode) || original.st_uid != ::geteuid()
					|| (original.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) != 0 || original.st_nlink != 1
					|| aGuard.FileIdentity != std::array<std::uint64_t, 4U>{ 1U, static_cast<std::uint64_t>(original.st_dev), 0U, static_cast<std::uint64_t>(original.st_ino) }
					|| ::flock(Guard, LOCK_SH | LOCK_NB) != 0
					|| ::lstat(path.c_str(), &named) != 0 || named.st_dev != original.st_dev || named.st_ino != original.st_ino
					|| ::socketpair(AF_UNIX, SOCK_STREAM, 0, Channel.data()) != 0
					|| ::fcntl(Channel[0], F_SETFD, FD_CLOEXEC) != 0
					|| ::fcntl(Channel[1], F_SETFD, FD_CLOEXEC) != 0) {
					return false;
				}
				const auto channel = Channel[1];
				const auto guard   = Guard;
				aProcess.setChildProcessModifier([started = &aProcess, channel, guard] {
					static_cast<void>(::umask(S_IRWXG | S_IRWXO));
					if (::dup2(channel, 3) < 0 || ::fcntl(3, F_SETFD, 0) != 0
						|| ::dup2(guard, 4) < 0 || ::fcntl(4, F_SETFD, 0) != 0) {
						started->failChildProcessModifier("Git workspace lifetime", errno);
					}
					// [[BPR-1252]]. After the two inherited descriptors exist,
					// so the bound counts them.
					bpr::platform::unix_descriptor_sweep::bound();
				});
				aProcess.setUnixProcessParameters({
					.flags = QProcess::UnixProcessFlag::CloseFileDescriptors
						| QProcess::UnixProcessFlag::DisableCoreDumps
						| QProcess::UnixProcessFlag::ResetSignalHandlers
						| QProcess::UnixProcessFlag::CreateNewSession,
					.lowestFileDescriptorToClose = 5,
				});
				auto arguments = aProcess.arguments();
				arguments.prepend(aProcess.program());
				arguments.prepend("--workspace-lease");
	#ifdef BPR_AGENT_SUPERVISOR_EXECUTABLE
				aProcess.setProgram(QString::fromUtf8(BPR_AGENT_SUPERVISOR_EXECUTABLE));
	#else
				aProcess.setProgram(QCoreApplication::applicationDirPath() + "/better_pull_review_agent_supervisor");
	#endif
				aProcess.setArguments(arguments);
				Supervised = true;
				return true;
#elif defined(Q_OS_WIN)
				const auto path = std::filesystem::path{ std::u8string{ aGuard.Path.begin(), aGuard.Path.end() } };
				if (aGuard.Path.empty() || aGuard.Path.size() > 4096U
					|| aGuard.Path.find('\0') != std::string::npos || !path.is_absolute()) {
					return false;
				}
				// The guard opens without following a reparse point and binds the
				// recorded identity and the private descriptor on the handle.
				GuardPath     = path.wstring();
				GuardIdentity = aGuard.FileIdentity;
				Guard         = adapters::agent::detail::open_windows_workspace_guard(GuardPath, GuardIdentity, adapters::agent::detail::windows_guard_lock::shared);
				if (!Guard || !make_pipe(LifetimeOwner, LifetimeChild, true, lifetime_pipe_bytes)
					|| !make_pipe(ReportOwner, ReportChild, false, 0U)) {
					return false;
				}
	#ifdef BPR_AGENT_SUPERVISOR_EXECUTABLE
				SupervisorPath = std::filesystem::path{ BPR_AGENT_SUPERVISOR_EXECUTABLE }.make_preferred().wstring();
	#else
				SupervisorPath = std::filesystem::path{ (QCoreApplication::applicationDirPath() + "/better_pull_review_agent_supervisor.exe").toStdWString() }.make_preferred().wstring();
	#endif
				SupervisorDirectory = std::filesystem::path{ SupervisorPath }.parent_path().wstring();
				GitExecutable       = std::filesystem::path{ aProcess.program().toStdWString() }.make_preferred().wstring();
				SupervisorCommand   = L"\"" + SupervisorPath + L"\" --workspace-owner "
					+ std::to_wstring(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(LifetimeChild.get()))) + L" "
					+ std::to_wstring(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ReportChild.get())));
				if (GitExecutable.empty() || SupervisorDirectory.empty() || GitExecutable.find(L'"') != std::wstring::npos || SupervisorPath.find(L'"') != std::wstring::npos) {
					return false;
				}
				// Supervised is set only once the launch is redirected, so a start
				// that bypassed the modifier is refused rather than run unguarded.
				aProcess.setCreateProcessArgumentsModifier([this](QProcess::CreateProcessArguments* const aArguments) {
					redirect(aArguments);
				});
				return true;
#else
				static_cast<void>(aProcess);
				static_cast<void>(aGuard);
				return false;
#endif
			}
		};

		[[nodiscard]] auto await_supervisor(QProcess& aProcess) -> bool
		{
			// The owner channel has closed. Never kill the lock-holding helper:
			// even after our bounded wait expires it must retain exclusion until
			// it can stop and reap Git. The worker quarantines an unreaped helper.
			return aProcess.state() == QProcess::NotRunning
				|| aProcess.waitForFinished(static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(reap_timeout).count()));
		}

		struct process_execution
		{
			process_result            Result;
			std::unique_ptr<QProcess> AwaitingReap;
			qint64                    ProcessGroupId = 0;
			bool                      Supervised     = false;
		};

		/// \brief Kills Git's tree, its Windows job first, and reaps it.
		[[nodiscard]] auto kill_and_reap(
			QProcess&    aProcess,
			windows_job& aJob,
			qint64       aProcessGroupId) -> bool
		{
			return process_support::kill_process_group_and_reap(
				aProcess,
				aProcessGroupId,
				reap_timeout,
				[&aJob](QProcess& aChild, const qint64 aGroup) {
					aJob.terminate();
					process_support::kill_process_tree(aChild, aGroup);
				});
		}

		struct runner_configuration
		{
			std::filesystem::path                           Executable;
			std::filesystem::path                           WorkingDirectory;
			std::filesystem::path                           Home;
			std::filesystem::path                           Xdg;
			std::filesystem::path                           Temporary;
			std::filesystem::path                           SystemConfig;
			std::filesystem::path                           GlobalConfig;
			std::filesystem::path                           Attributes;
			std::filesystem::path                           Templates;
			std::filesystem::path                           Hooks;
			std::optional<std::filesystem::path>            ExecPath;
			std::optional<std::filesystem::path>            WindowsSystemRoot;
			std::shared_ptr<application::insights_observer> Insights;
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
			git_process_error          aFailure,
			git_workspace_lifetime&    aLifetime) -> process_execution
		{
			aLifetime.close_owner();
			if (aLifetime.Supervised ? await_supervisor(*aProcess) : kill_and_reap(*aProcess, aJob, aProcessGroupId)) {
				return {
					.Result         = std::unexpected{ std::move(aFailure) },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			static_cast<void>(QObject::disconnect(aProcess.get(), nullptr, aProcess.get(), nullptr));
			return {
				.Result         = std::unexpected{ git_process_failure::reap_failure },
				.AwaitingReap   = std::move(aProcess),
				.ProcessGroupId = aProcessGroupId,
				.Supervised     = aLifetime.Supervised,
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
			auto        environment = process_environment(aConfiguration);
			for (const auto& [name, value] : aRequest.Environment) {
				QString decodedName;
				QString decodedValue;
				if (!decode_text(name, decodedName) || !decode_text(value, decodedValue) || environment.contains(decodedName)) {
					environment = QProcessEnvironment{ };
					break;
				}
				environment.insert(decodedName, decodedValue);
			}
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

			qint64                 processGroupId = 0;
			auto                   process        = std::make_unique<QProcess>();
			windows_job            job;
			git_workspace_lifetime lifetime;
#if defined(Q_OS_WIN)
			launch_restriction inheritance;
#endif
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
						// [[BPR-1252]]. Git runs once per command, so its
						// sweep must not cost one call per allowed descriptor.
						bpr::platform::unix_descriptor_sweep::bound();
					});
#endif
				if (aRequest.WorkspaceGuard && !lifetime.prepare(*process, *aRequest.WorkspaceGuard)) {
					return failed_execution(process, job, processGroupId, git_process_failure::invalid_configuration, lifetime);
				}
#if defined(Q_OS_WIN)
				if (!aRequest.WorkspaceGuard) {
					// [[BPR-1265]]. The job exists before the launch, and the
					// launch names it, so Git is created inside it.
					if (!job.create()) {
						return failed_execution(process, job, processGroupId, git_process_failure::process_start, lifetime);
					}
					inheritance.place_in_job(job.native());
					restrict_to_standard_handles(*process, inheritance, aConfiguration.Home.parent_path());
				}
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
				QObject::connect(process.get(), &QProcess::started, process.get(), [processPointer = process.get(), insights = aConfiguration.Insights, supervised = lifetime.Supervised]() noexcept {
						try {
							if (insights) {
								const auto capture = application::capture_insights_child(insights,1);
								process_support::observe_reaped_cpu(*processPointer,[capture,supervised](auto aCpu) { capture.record(supervised ? std::nullopt : aCpu); });
							}
						} catch (...) {} }, Qt::DirectConnection);
				QObject::connect(
					process.get(),
					&QProcess::finished,
					process.get(),
					[&processGroupId, supervised = lifetime.Supervised](
						const int,
						const QProcess::ExitStatus) noexcept {
						if (!supervised) {
							kill_process_group(processGroupId);
						}
					},
					Qt::DirectConnection);

				std::string standardOutput;
				standardOutput.reserve(std::min<std::size_t>(aRequest.StandardOutputLimit, 64U * 1024U));
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
						std::move(aFailure), lifetime);
				};

				const auto startDeadline = std::min(aDeadline, clock::now() + start_timeout);
				process->start();
				lifetime.close_child();
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
#if defined(Q_OS_WIN)
				// A list that could not be built never gets here: process creation
				// refused the start. This catches a child created without the
				// restriction all the same, by a Qt that never called the modifier
				// or a process creation that accepted the refused flags. It must
				// not run.
				if (!aRequest.WorkspaceGuard && !inheritance.placed_in_job()) {
					return failed(git_process_failure::process_start);
				}
	#if defined(BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION)
				// [[BPR-1265]]. The contract's seam for the race: a slow parent
				// reaches the job check after a quick Git has already exited.
				if (std::error_code delayError; std::filesystem::exists(aConfiguration.Home.parent_path() / "inject-delay-before-job-check", delayError) && !delayError) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
				}
	#endif
#endif
				if (aRequest.WorkspaceGuard) {
					// The supervisor owns Git's lifetime; attaching it to this
					// process's kill-on-close job would end it with this process.
					if (!lifetime.Supervised) {
						return failed(git_process_failure::process_start);
					}
					for (;;) {
						const auto start = lifetime.poll_start();
						if (start == git_workspace_lifetime::start_state::started) {
							break;
						}
						if (start == git_workspace_lifetime::start_state::failed || clock::now() >= startDeadline) {
							return failed(git_process_failure::process_start);
						}
						if (const auto failure = terminalFailure(); failure.has_value()) {
							return failed(*failure);
						}
						std::this_thread::sleep_for(wait_slice);
					}
				} else if (
					process->state() != QProcess::NotRunning
					&& !job.holds(process->processId())) {
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
				if (!lifetime.Supervised) {
					kill_process_group(processGroupId);
				}
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
					auto diagnostic = redacted_diagnostic(standardError);
					if (aRequest.SummarizeFailedOutput != nullptr && !standardOutput.empty()) {
						diagnostic = with_output_summary(
							std::move(diagnostic),
							aRequest.SummarizeFailedOutput(standardOutput));
					}
					return failed(
						git_process_error{
							git_process_failure::nonzero_exit,
							std::move(diagnostic) });
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
					git_process_failure::process_start, lifetime);
			}
		}

		struct process_work_item
			: process_support::process_work_state<process_result>
		{
			process_request Request;
		};

		/// \brief What the shared lane runs for this runner ([[BPR-1436]]).
		struct process_lane_traits
		{
			using item      = process_work_item;
			using result    = process_result;
			using execution = process_execution;

			[[nodiscard]] static auto failure(
				const process_support::lane_failure aFailure) -> process_result
			{
				switch (aFailure) {
				case process_support::lane_failure::cancelled:
					return std::unexpected{ git_process_failure::cancelled };
				case process_support::lane_failure::timeout:
					return std::unexpected{ git_process_failure::timeout };
				case process_support::lane_failure::reap_failure:
					return std::unexpected{ git_process_failure::reap_failure };
				case process_support::lane_failure::start_failure:
					break;
				}
				return std::unexpected{ git_process_failure::process_start };
			}

			[[nodiscard]] auto execute(
				process_work_item&      aItem,
				const std::atomic_bool& aShutdownRequested) const -> process_execution
			{
				return execute_process(
					Configuration,
					aItem.Request,
					aItem.Stop,
					aShutdownRequested,
					aItem.Deadline);
			}

			/// \brief The bounded last attempt on a Git the launch could not reap.
			/// \details A supervised Git's helper is never killed: it keeps its
			/// exclusion until it has stopped and reaped Git itself.
			[[nodiscard]] static auto finish_reap(process_execution& aExecution) -> bool
			{
				if (aExecution.Supervised) {
					return await_supervisor(*aExecution.AwaitingReap);
				}
				return process_support::finish_background_reap(
					*aExecution.AwaitingReap,
					aExecution.ProcessGroupId,
					reap_timeout);
			}

			runner_configuration Configuration;
		};

		class process_worker final
		{
		public:
			explicit process_worker(runner_configuration aConfiguration)
				: Lane{ process_lane_traits{ .Configuration = std::move(aConfiguration) } }
			{
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
				return Lane.submit(
					item,
					std::chrono::duration_cast<std::chrono::milliseconds>(reap_timeout));
			}

		private:
			process_support::serialized_process_lane<process_lane_traits> Lane;
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
			static_cast<void>(aSource);
			static_cast<void>(aDestination);
			aError = std::make_error_code(
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
					aPath = std::move(quarantine);

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

#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
	#if defined(Q_OS_WIN)
		namespace win_guard                         = adapters::agent::detail;
		using journal_native                        = HANDLE;
		inline const journal_native journal_invalid = INVALID_HANDLE_VALUE;
	#else
		using journal_native                            = int;
		inline constexpr journal_native journal_invalid = -1;
	#endif
		void close_journal(const journal_native aValue) noexcept
		{
			if (aValue != journal_invalid) {
	#if defined(Q_OS_WIN)
				static_cast<void>(CloseHandle(aValue));
	#else
				static_cast<void>(::close(aValue));
	#endif
			}
		}
		class workspace_descriptor
		{
		public:
			explicit workspace_descriptor(journal_native aValue)
				: Value{ aValue }
			{ }
			~workspace_descriptor() { close_journal(Value); }
			workspace_descriptor(const workspace_descriptor&)                                  = delete;
			auto               operator=(const workspace_descriptor&) -> workspace_descriptor& = delete;
			[[nodiscard]] auto get() const noexcept -> journal_native { return Value; }
			[[nodiscard]] auto valid() const noexcept -> bool { return Value != journal_invalid; }
			[[nodiscard]] auto release() noexcept -> journal_native { return std::exchange(Value, journal_invalid); }

		private:
			journal_native Value;
		};
		void workspace_checkpoint([[maybe_unused]] std::string_view aName)
		{
	#if defined(BPR_WORKSPACE_OWNERSHIP_FAULT_INJECTION)
			installed_workspace_checkpoint(aName);
	#endif
		}
		[[nodiscard]] auto absent(const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			return error == std::errc::no_such_file_or_directory || (!error && status.type() == std::filesystem::file_type::not_found);
		}
		using ownership_identity                              = std::array<std::uint64_t, 4U>;
		constexpr std::size_t maximum_workspace_records       = 64U;
		constexpr std::size_t maximum_workspace_journal_bytes = 64U * 1024U;

	#if defined(Q_OS_WIN)
		// [[BPR-0622]]. The Windows journal primitives. Privacy is the exact
		// protected current-user descriptor the journal and its directories are
		// created with, read back from the opened handle; the weaker path-level
		// private_directory check alone does not prove it. Locks are the fixed
		// guard range the launcher and supervisor share.
		[[nodiscard]] auto windows_object_private(const std::filesystem::path& aPath, const bool aDirectory) -> bool
		{
			const auto principals = win_guard::windows_private_principals::load();
			if (!principals) {
				return false;
			}
			const auto handle = win_guard::windows_handle{ CreateFileW(
				aPath.c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			return static_cast<bool>(handle) && win_guard::windows_private_to_current_user(handle.get(), *principals, aDirectory);
		}
		[[nodiscard]] auto journal_lock(const journal_native aHandle, const bool aExclusive) -> bool
		{
			auto overlapped       = OVERLAPPED{ };
			overlapped.Offset     = static_cast<DWORD>(win_guard::windows_workspace_guard_lock_offset & 0xFFFF'FFFFULL);
			overlapped.OffsetHigh = static_cast<DWORD>(win_guard::windows_workspace_guard_lock_offset >> 32U);
			return LockFileEx(aHandle, LOCKFILE_FAIL_IMMEDIATELY | (aExclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0U), 0U, 1U, 0U, &overlapped) != FALSE;
		}
		[[nodiscard]] auto journal_unlock(const journal_native aHandle) -> bool
		{
			auto overlapped       = OVERLAPPED{ };
			overlapped.Offset     = static_cast<DWORD>(win_guard::windows_workspace_guard_lock_offset & 0xFFFF'FFFFULL);
			overlapped.OffsetHigh = static_cast<DWORD>(win_guard::windows_workspace_guard_lock_offset >> 32U);
			return UnlockFileEx(aHandle, 0U, 1U, 0U, &overlapped) != FALSE;
		}
		[[nodiscard]] auto lock_busy() -> bool
		{
			return GetLastError() == ERROR_LOCK_VIOLATION;
		}
	#else
		[[nodiscard]] auto journal_lock(const journal_native aDescriptor, const bool aExclusive) -> bool
		{
			return ::flock(aDescriptor, (aExclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0;
		}
		[[nodiscard]] auto lock_busy() -> bool
		{
			return errno == EWOULDBLOCK || errno == EAGAIN;
		}
	#endif

		/// The owner holds a shared lock; cleanup needs exclusive access. flock
		/// converts in place and can drop the shared lock on failure; Windows
		/// releases and retakes. Either way the registry boundary lock the caller
		/// holds excludes recovery until shared ownership is restored.
		[[nodiscard]] auto upgrade_journal_lock(const journal_native aHandle, bool& aPoisoned) -> bool
		{
	#if defined(Q_OS_WIN)
			if (!journal_unlock(aHandle)) {
				aPoisoned = true;
				return false;
			}
	#endif
			if (journal_lock(aHandle, true)) {
				return true;
			}
			if (!journal_lock(aHandle, false)) {
				aPoisoned = true;
			}
			return false;
		}

		[[nodiscard]] auto workspace_private_directory(const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			return private_directory(aPath) && windows_object_private(aPath, true);
	#else
			return private_directory(aPath);
	#endif
		}
		[[nodiscard]] auto workspace_empty_private_directory(const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			return workspace_private_directory(aPath) && std::filesystem::is_empty(aPath, error) && !error;
		}
		[[nodiscard]] auto workspace_private_file(const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			return private_regular_file(aPath) && windows_object_private(aPath, false);
	#else
			return private_regular_file(aPath);
	#endif
		}

		/// Exclusive creation of one private directory; never adopts an existing one.
		[[nodiscard]] auto create_workspace_directory(const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			const auto principals = win_guard::windows_private_principals::load();
			auto       descriptor = principals ? win_guard::windows_private_descriptor::make(*principals, true) : std::nullopt;
			return descriptor && CreateDirectoryW(aPath.c_str(), descriptor->attributes()) != FALSE;
	#else
			return ::mkdir(aPath.c_str(), S_IRWXU) == 0;
	#endif
		}

		[[nodiscard]] auto sync_workspace_directory(const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			// NTFS flushes a directory's metadata through a handle opened for
			// writing with backup semantics.
			const auto handle = win_guard::windows_handle{ CreateFileW(
				aPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
			return static_cast<bool>(handle) && FlushFileBuffers(handle.get()) != FALSE;
	#else
			const auto fd = ::open(aPath.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
			if (fd < 0) {
				return false;
			}
			const auto result = ::fsync(fd) == 0;
			static_cast<void>(::close(fd));
			return result;
	#endif
		}

		struct journal_status
		{
			ownership_identity File{ };
			std::uint64_t      Size    = 0U;
			bool               Private = false;
		};
		/// Exact identity, size and privacy of an open journal: a single-link
		/// regular file owned privately by the current user.
		[[nodiscard]] auto journal_stat(const journal_native aHandle) -> std::optional<journal_status>
		{
	#if defined(Q_OS_WIN)
			const auto identity   = win_guard::windows_regular_file_identity(aHandle);
			const auto principals = win_guard::windows_private_principals::load();
			auto       size       = LARGE_INTEGER{ };
			if (!identity || !principals || GetFileSizeEx(aHandle, &size) == FALSE || size.QuadPart < 0) {
				return std::nullopt;
			}
			return journal_status{
				.File    = *identity,
				.Size    = static_cast<std::uint64_t>(size.QuadPart),
				.Private = win_guard::windows_private_to_current_user(aHandle, *principals, false),
			};
	#else
			struct stat info{ };
			if (::fstat(aHandle, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_size < 0) {
				return std::nullopt;
			}
			return journal_status{
				.File    = { 1U, static_cast<std::uint64_t>(info.st_dev), 0U, static_cast<std::uint64_t>(info.st_ino) },
				.Size    = static_cast<std::uint64_t>(info.st_size),
				.Private = info.st_uid == ::geteuid() && (info.st_mode & static_cast<mode_t>(S_IRWXG | S_IRWXO)) == 0,
			};
	#endif
		}
		[[nodiscard]] auto journal_read(const journal_native aHandle, std::string& aBytes) -> bool
		{
			std::size_t read = 0U;
			while (read < aBytes.size()) {
	#if defined(Q_OS_WIN)
				auto overlapped       = OVERLAPPED{ };
				overlapped.Offset     = static_cast<DWORD>(read & 0xFFFF'FFFFU);
				overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(read) >> 32U);
				auto count            = DWORD{ 0U };
				if (ReadFile(aHandle, aBytes.data() + read, static_cast<DWORD>(aBytes.size() - read), &count, &overlapped) == FALSE || count == 0U) {
					return false;
				}
	#else
				const auto count = ::pread(aHandle, aBytes.data() + read, aBytes.size() - read, static_cast<off_t>(read));
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					return false;
				}
	#endif
				read += static_cast<std::size_t>(count);
			}
			return true;
		}
		[[nodiscard]] auto journal_truncate(const journal_native aHandle, const std::uint64_t aSize) -> bool
		{
	#if defined(Q_OS_WIN)
			auto position     = LARGE_INTEGER{ };
			position.QuadPart = static_cast<LONGLONG>(aSize);
			return SetFilePointerEx(aHandle, position, nullptr, FILE_BEGIN) != FALSE && SetEndOfFile(aHandle) != FALSE
				&& FlushFileBuffers(aHandle) != FALSE;
	#else
			return ::ftruncate(aHandle, static_cast<off_t>(aSize)) == 0 && ::fsync(aHandle) == 0;
	#endif
		}
		/// Appends within the journal bound and makes it durable. A partial or
		/// unflushed write poisons the owner.
		[[nodiscard]] auto journal_append(const journal_native aHandle, const std::string_view aBytes, bool& aPoisoned) -> bool
		{
	#if defined(Q_OS_WIN)
			auto end = LARGE_INTEGER{ };
			if (SetFilePointerEx(aHandle, LARGE_INTEGER{ }, &end, FILE_END) == FALSE || end.QuadPart < 0
				|| static_cast<std::uint64_t>(end.QuadPart) + aBytes.size() > maximum_workspace_journal_bytes) {
				return false;
			}
			std::size_t written = 0U;
			while (written < aBytes.size()) {
				auto count = DWORD{ 0U };
				if (WriteFile(aHandle, aBytes.data() + written, static_cast<DWORD>(aBytes.size() - written), &count, nullptr) == FALSE || count == 0U) {
					aPoisoned = true;
					return false;
				}
				written += count;
			}
			if (FlushFileBuffers(aHandle) == FALSE) {
				aPoisoned = true;
				return false;
			}
			return true;
	#else
			const auto offset = ::lseek(aHandle, 0, SEEK_END);
			if (offset < 0 || static_cast<std::uint64_t>(offset) + aBytes.size() > maximum_workspace_journal_bytes) {
				return false;
			}
			std::size_t written = 0U;
			while (written < aBytes.size()) {
				const auto count = ::write(aHandle, aBytes.data() + written, aBytes.size() - written);
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count <= 0) {
					aPoisoned = true;
					return false;
				}
				written += static_cast<std::size_t>(count);
			}
			if (::fsync(aHandle) != 0) {
				aPoisoned = true;
				return false;
			}
			return true;
	#endif
		}
		/// Opens an existing journal for the owner or recovery without following
		/// a link. Windows shares read and write with the launcher and supervisor
		/// guard opens, and delete so a completed journal can be removed.
		[[nodiscard]] auto open_journal(const std::filesystem::path& aPath) -> journal_native
		{
	#if defined(Q_OS_WIN)
			return CreateFileW(
				aPath.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	#else
			return ::open(aPath.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	#endif
		}
		enum class journal_creation : std::uint8_t {
			created,
			exists,
			failed,
		};
		[[nodiscard]] auto create_journal(const std::filesystem::path& aPath, journal_native& aHandle) -> journal_creation
		{
	#if defined(Q_OS_WIN)
			const auto principals = win_guard::windows_private_principals::load();
			auto       descriptor = principals ? win_guard::windows_private_descriptor::make(*principals, false) : std::nullopt;
			if (!descriptor) {
				return journal_creation::failed;
			}
			aHandle = CreateFileW(
				aPath.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, descriptor->attributes(),
				CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			if (aHandle == INVALID_HANDLE_VALUE) {
				return GetLastError() == ERROR_FILE_EXISTS ? journal_creation::exists : journal_creation::failed;
			}
			return journal_creation::created;
	#else
			aHandle = ::open(aPath.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
			if (aHandle < 0) {
				return errno == EEXIST ? journal_creation::exists : journal_creation::failed;
			}
			return journal_creation::created;
	#endif
		}
		/// Removes the completed journal while its lock is still held. Windows
		/// uses POSIX delete semantics, so the name disappears at once.
		[[nodiscard]] auto delete_journal(const journal_native aHandle, const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			static_cast<void>(aPath);
			auto disposition  = FILE_DISPOSITION_INFO_EX{ };
			disposition.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS;
			return SetFileInformationByHandle(aHandle, FileDispositionInfoEx, &disposition, sizeof(disposition)) != FALSE;
	#else
			static_cast<void>(aHandle);
			return ::unlink(aPath.c_str()) == 0;
	#endif
		}
		/// The neighbourhood's parent may not be writable by anyone but this user
		/// (and, on Windows, the package, SYSTEM and Administrators, the analogue of
		/// root), so nobody else can swap the neighbourhood out from under it.
		[[nodiscard]] auto workspace_parent_trusted(const std::filesystem::path& aParent) -> bool
		{
	#if defined(Q_OS_WIN)
			if (canonical_existing(aParent) != aParent || !private_directory(aParent)) {
				return false;
			}
			const auto           principals = win_guard::windows_private_principals::load();
			const auto           handle     = win_guard::windows_handle{ CreateFileW(
				aParent.c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr) };
			PSID                 owner      = nullptr;
			PACL                 dacl       = nullptr;
			PSECURITY_DESCRIPTOR descriptor = nullptr;
			if (!principals || !handle
				|| GetSecurityInfo(handle.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl, nullptr, &descriptor)
					!= ERROR_SUCCESS) {
				return false;
			}
			const auto freed = std::unique_ptr<void, decltype(&LocalFree)>{ descriptor, &LocalFree };
			if (owner == nullptr || EqualSid(owner, principals->user()) == FALSE || dacl == nullptr) {
				return false;
			}
			// OWNER RIGHTS grants only to the object's owner, which is this user;
			// CPython's private mkdir writes exactly SYSTEM, Administrators and it.
			std::array<BYTE, SECURITY_MAX_SID_SIZE> system{ };
			std::array<BYTE, SECURITY_MAX_SID_SIZE> administrators{ };
			std::array<BYTE, SECURITY_MAX_SID_SIZE> ownerRights{ };
			auto                                    systemSize         = static_cast<DWORD>(system.size());
			auto                                    administratorsSize = static_cast<DWORD>(administrators.size());
			auto                                    ownerRightsSize    = static_cast<DWORD>(ownerRights.size());
			if (CreateWellKnownSid(WinLocalSystemSid, nullptr, system.data(), &systemSize) == FALSE
				|| CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators.data(), &administratorsSize) == FALSE
				|| CreateWellKnownSid(WinCreatorOwnerRightsSid, nullptr, ownerRights.data(), &ownerRightsSize) == FALSE) {
				return false;
			}
			constexpr auto write_rights = static_cast<ACCESS_MASK>(
				FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER
				| GENERIC_WRITE | GENERIC_ALL);
			for (DWORD index = 0U; index < dacl->AceCount; ++index) {
				void* entry = nullptr;
				if (GetAce(dacl, index, &entry) == FALSE) {
					return false;
				}
				const auto* const header = static_cast<ACE_HEADER*>(entry);
				// [[BPR-0825]]. A deny entry can only narrow access. Every other type
				// that is not a plain allow (object, callback, compound or unknown)
				// may grant write to a principal this walk cannot identify, so the
				// parent is refused rather than trusted past it.
				if (header->AceType == ACCESS_DENIED_ACE_TYPE || header->AceType == ACCESS_DENIED_OBJECT_ACE_TYPE
					|| header->AceType == ACCESS_DENIED_CALLBACK_ACE_TYPE || header->AceType == ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE) {
					continue;
				}
				if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
					return false;
				}
				const auto* const allowed = static_cast<ACCESS_ALLOWED_ACE*>(entry);
				const auto        sid     = static_cast<PSID>(static_cast<void*>(const_cast<DWORD*>(&allowed->SidStart)));
				if ((allowed->Mask & write_rights) == 0U || EqualSid(sid, principals->user()) != FALSE
					|| (principals->container() != nullptr && EqualSid(sid, principals->container()) != FALSE)
					|| EqualSid(sid, system.data()) != FALSE || EqualSid(sid, administrators.data()) != FALSE
					|| EqualSid(sid, ownerRights.data()) != FALSE) {
					continue;
				}
				return false;
			}
			return true;
	#else
			struct stat info{ };
			return canonical_existing(aParent) == aParent && ::lstat(aParent.c_str(), &info) == 0 && S_ISDIR(info.st_mode)
				&& info.st_uid == ::geteuid() && (info.st_mode & static_cast<mode_t>(S_IWGRP | S_IWOTH)) == 0;
	#endif
		}
		/// The registry and neighbourhood roots themselves: created private, or an
		/// existing one accepted only when it is private.
		[[nodiscard]] auto make_workspace_root_directory(const std::filesystem::path& aPath) -> bool
		{
	#if defined(Q_OS_WIN)
			if (!absent(aPath)) {
				return workspace_private_directory(aPath);
			}
			if (!create_workspace_directory(aPath) && GetLastError() != ERROR_ALREADY_EXISTS) {
				return false;
			}
			return workspace_private_directory(aPath);
	#else
			return make_private_directory(aPath);
	#endif
		}
		struct workspace_record
		{
			std::uint32_t Version = 1U;
			// 0 reserved, 1 parent owned, 2 materializing, 3 ready,
			// 4 child cleanup planned, 5 parent cleanup planned, 6 removed.
			std::uint32_t                                 Phase = 0U;
			std::string                                   Token;
			ownership_identity                            Registry{ };
			ownership_identity                            Neighbourhood{ };
			ownership_identity                            File{ };
			ownership_identity                            Parent{ };
			ownership_identity                            Child{ };
			std::array<std::uint64_t, 10U>                Scope{ };
			std::array<std::array<std::uint8_t, 32U>, 3U> AuthorityFingerprints{ };
			std::string                                   Head;
			auto                                          operator<=>(const workspace_record&) const = default;
			struct glaze
			{
				using self                  = workspace_record;
				static constexpr auto value = glz::object(
					"Version", &self::Version, "Phase", &self::Phase, "Token", &self::Token,
					"Registry", &self::Registry, "Neighbourhood", &self::Neighbourhood,
					"File", &self::File, "Parent", &self::Parent, "Child", &self::Child,
					"Scope", &self::Scope, "AuthorityFingerprints", &self::AuthorityFingerprints, "Head", &self::Head);
			};
		};

		[[nodiscard]] auto ownership_id(const std::filesystem::path& aPath, exact_artifact_kind aKind) -> ownership_identity
		{
			const auto identity = exact_identity(aPath, aKind);
			return identity ? workspace_directory_identity(*identity).Opaque : ownership_identity{ };
		}
		[[nodiscard]] auto record_transition(const workspace_record& aOld, const workspace_record& aNew) -> bool
		{
			const auto oldPhase = aOld.Phase;
			const auto phase    = aNew.Phase;
			if (!((phase == oldPhase + 1U && oldPhase < 6U)
				  || (oldPhase == 0U && phase == 6U) || (oldPhase == 1U && phase == 5U)
				  || (oldPhase == 2U && phase == 4U))) {
				return false;
			}
			auto adjusted  = aNew;
			adjusted.Phase = oldPhase;
			if (oldPhase == 0U && phase == 1U) {
				if (!application::valid(application::agent_workspace_directory_identity{ aNew.Parent }) || aNew.Parent[0] != 2U) {
					return false;
				}
				adjusted.Parent = aOld.Parent;
			}
			if (oldPhase == 1U && phase == 2U) {
				if (!application::valid(application::agent_workspace_directory_identity{ aNew.Child }) || aNew.Child[0] != 2U) {
					return false;
				}
				adjusted.Child = aOld.Child;
			}
			return adjusted == aOld;
		}

		class workspace_ownership final : public installed_agent_workspace_ownership
		{
		public:
			workspace_ownership(std::filesystem::path aRegistry, std::filesystem::path aNeighbourhood, workspace_record aRecord, journal_native aDescriptor)
				: Record{ std::move(aRecord) }
				, Registry{ std::move(aRegistry) }
				, Neighbourhood{ std::move(aNeighbourhood) }
				, Descriptor{ aDescriptor }
			{
				Journal                = Registry / (Record.Token + ".jsonl");
				Layout.Parent          = Neighbourhood / ("betterpullreview-agent-" + Record.Token);
				Layout.Workspace       = Layout.Parent / "workspace";
				Layout.Guard           = { .Path = path_text(Journal), .FileIdentity = Record.File };
				Layout.OwnershipMarker = "bpr-agent-workspace-v1:" + std::to_string(Record.Scope[0]) + ":" + std::to_string(Record.Scope[4]) + ":" + Record.Head + "\n";
				refresh_identities();
			}
			~workspace_ownership() override
			{
				close_journal(Descriptor);
			}
			[[nodiscard]] auto layout() const noexcept -> const installed_agent_workspace_layout& override { return Layout; }
			[[nodiscard]] auto boundary_valid() const -> bool
			{
				return !Poisoned && workspace_private_directory(Registry) && workspace_private_directory(Neighbourhood)
					&& ownership_id(Registry, exact_artifact_kind::directory) == Record.Registry
					&& ownership_id(Neighbourhood, exact_artifact_kind::directory) == Record.Neighbourhood
					&& workspace_private_file(Journal) && ownership_id(Journal, exact_artifact_kind::regular_file) == Record.File;
			}
			[[nodiscard]] auto append(workspace_record aNext, bool aInitial = false) -> bool
			{
				if (!boundary_valid() || (!aInitial && !record_transition(Record, aNext))) {
					return false;
				}
				std::string bytes;
				if (glz::write_json(aNext, bytes)) {
					return false;
				}
				bytes += '\n';
				if (!journal_append(Descriptor, bytes, Poisoned)) {
					return false;
				}
				Record = std::move(aNext);
				refresh_identities();
				workspace_checkpoint("journal-" + std::to_string(Record.Phase));
				return true;
			}
			[[nodiscard]] auto advance(std::uint32_t aPhase) -> bool
			{
				auto next  = Record;
				next.Phase = aPhase;
				return append(std::move(next));
			}
			[[nodiscard]] auto mark_ready() -> bool override
			{
				auto boundary = process_boundary_lock{ Registry };
				return boundary.valid() && boundary_valid() && Record.Phase == 2U
					&& directories_match() && read_exact_file(Layout.Workspace / ".git" / "bpr-owned-workspace", Layout.OwnershipMarker) && advance(3U);
			}
			[[nodiscard]] auto cleanup() -> bool override
			{
				auto boundary = process_boundary_lock{ Registry };
				if (!boundary.valid()) {
					return false;
				}
				if (!upgrade_journal_lock(Descriptor, Poisoned)) {
					return false;
				}
				return cleanup_locked();
			}
			[[nodiscard]] auto cleanup_locked() -> bool
			{
				if (!boundary_valid()) {
					return false;
				}
				const auto parentTrash = Neighbourhood / (".bpr-cleanup-" + Record.Token);
				const auto childTrash  = Layout.Parent / ".bpr-cleanup-workspace";
				if (Record.Phase == 0U) {
					if (!absent(Layout.Parent) || !absent(parentTrash) || !advance(6U)) {
						return false;
					}
				}
				if (Record.Phase == 1U) {
					if (ownership_id(Layout.Parent, exact_artifact_kind::directory) != Record.Parent || !workspace_empty_private_directory(Layout.Parent) || !advance(5U)) {
						return false;
					}
				}
				if (Record.Phase == 2U || Record.Phase == 3U) {
					if (!directories_match() || (Record.Phase == 3U && !read_exact_file(Layout.Workspace / ".git" / "bpr-owned-workspace", Layout.OwnershipMarker)) || !advance(4U)) {
						return false;
					}
				}
				if (Record.Phase == 4U) {
					if (ownership_id(Layout.Parent, exact_artifact_kind::directory) != Record.Parent
						|| !remove_planned(Layout.Workspace, childTrash, Record.Child, true)
						|| !workspace_empty_private_directory(Layout.Parent) || !advance(5U)) {
						return false;
					}
				}
				if (Record.Phase == 5U && (!remove_planned(Layout.Parent, parentTrash, Record.Parent, false) || !advance(6U))) {
					return false;
				}
				if (Record.Phase != 6U || !absent(Layout.Parent) || !absent(parentTrash) || !boundary_valid()) {
					return false;
				}
				return delete_journal(Descriptor, Journal) && sync_workspace_directory(Registry);
			}
			[[nodiscard]] auto directories_match() const -> bool
			{
				return workspace_private_directory(Layout.Parent) && workspace_private_directory(Layout.Workspace)
					&& ownership_id(Layout.Parent, exact_artifact_kind::directory) == Record.Parent
					&& ownership_id(Layout.Workspace, exact_artifact_kind::directory) == Record.Child;
			}
			workspace_record Record;

		private:
			void refresh_identities()
			{
				Layout.ParentIdentity.Opaque    = Record.Parent;
				Layout.WorkspaceIdentity.Opaque = Record.Child;
			}
			[[nodiscard]] static auto remove_planned(const std::filesystem::path& aSource, const std::filesystem::path& aTrash, const ownership_identity& aIdentity, bool aRecursive) -> bool
			{
				if (!absent(aSource)) {
					if (ownership_id(aSource, exact_artifact_kind::directory) != aIdentity || !absent(aTrash)) {
						return false;
					}
					std::error_code error;
					if (!rename_no_replace(aSource, aTrash, error) || !sync_workspace_directory(aSource.parent_path())) {
						return false;
					}
				}
				if (absent(aTrash)) {
					return true;
				}
				workspace_checkpoint(aRecursive ? "child-quarantined" : "parent-quarantined");
				if (!workspace_private_directory(aTrash) || ownership_id(aTrash, exact_artifact_kind::directory) != aIdentity) {
					return false;
				}
				std::error_code error;
				if (aRecursive) {
					static_cast<void>(std::filesystem::remove_all(aTrash, error));
				} else {
					static_cast<void>(std::filesystem::remove(aTrash, error));
				}
				return !error && absent(aTrash) && sync_workspace_directory(aTrash.parent_path());
			}
			std::filesystem::path            Registry;
			std::filesystem::path            Neighbourhood;
			std::filesystem::path            Journal;
			journal_native                   Descriptor = journal_invalid;
			bool                             Poisoned   = false;
			installed_agent_workspace_layout Layout;
		};

		struct workspace_json_options : glz::opts
		{
			// Glaze discovers this option override by its exact library-defined name.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;
			constexpr workspace_json_options() { error_on_missing_keys = true; }
		};
		[[nodiscard]] auto load_workspace_record(journal_native aFd, const std::filesystem::path& aRegistry, const std::filesystem::path& aNeighbourhood, std::string_view aToken) -> std::optional<workspace_record>
		{
			const auto status = journal_stat(aFd);
			if (!status || !status->Private || status->Size == 0U || status->Size > maximum_workspace_journal_bytes) {
				return std::nullopt;
			}
			std::string bytes(static_cast<std::size_t>(status->Size), '\0');
			if (!journal_read(aFd, bytes)) {
				return std::nullopt;
			}
			std::optional<workspace_record> previous;
			std::size_t                     offset = 0U;
			while (offset < bytes.size()) {
				const auto end = bytes.find('\n', offset);
				if (end == std::string::npos) {
					break;
				}
				workspace_record record;
				if (glz::read<workspace_json_options{ }>(record, std::string_view{ bytes }.substr(offset, end - offset))
					|| record.Version != 1U || record.Token != aToken || record.Registry != ownership_id(aRegistry, exact_artifact_kind::directory)
					|| record.Neighbourhood != ownership_id(aNeighbourhood, exact_artifact_kind::directory)
					|| record.File != status->File
					|| (record.Head.size() != 40U && record.Head.size() != 64U)
					|| !std::ranges::all_of(record.Head, [](char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f'); })
					|| std::ranges::any_of(record.Scope, [](auto aId) { return aId == 0U; })
					|| std::ranges::any_of(record.AuthorityFingerprints, [](const auto& aFingerprint) { return std::ranges::all_of(aFingerprint, [](auto aByte) { return aByte == 0U; }); })
					|| (previous ? !record_transition(*previous, record) : record.Phase != 0U || record.Parent != ownership_identity{ } || record.Child != ownership_identity{ })) {
					return std::nullopt;
				}
				previous = std::move(record);
				offset   = end + 1U;
			}
			if (!previous || (offset != bytes.size() && !journal_truncate(aFd, offset))) {
				return std::nullopt;
			}
			return previous;
		}
		[[nodiscard]] auto prepare_workspace_neighbourhood(const std::filesystem::path& aPath) -> bool
		{
			if (!absent(aPath)) {
				return workspace_private_directory(aPath);
			}
			const auto parent = aPath.parent_path();
			if (!workspace_parent_trusted(parent)) {
				return false;
			}
			if (!create_workspace_directory(aPath) && absent(aPath)) {
				return false;
			}
			return workspace_private_directory(aPath) && sync_workspace_directory(parent);
		}
		[[nodiscard]] auto prepare_workspace_registry(const std::filesystem::path& aRegistry, const std::filesystem::path& aNeighbourhood) -> bool
		{
			return aRegistry.is_absolute() && path_text(aRegistry).size() <= 3500U
				&& path_text(aRegistry).find('\0') == std::string::npos
				&& aRegistry == aRegistry.lexically_normal()
				&& path_text(aNeighbourhood).size() <= 3500U
				&& path_text(aNeighbourhood).find('\0') == std::string::npos
				&& aNeighbourhood.is_absolute() && aNeighbourhood == aNeighbourhood.lexically_normal()
				&& aRegistry != aNeighbourhood
				&& !path_begins_with(aNeighbourhood, aRegistry)
				&& prepare_workspace_neighbourhood(aNeighbourhood)
				&& workspace_private_directory(aRegistry.parent_path())
				&& make_workspace_root_directory(aRegistry)
				&& sync_workspace_directory(aRegistry.parent_path());
		}
		[[nodiscard]] auto recover_workspaces_locked(const std::filesystem::path& aRegistry, const std::filesystem::path& aNeighbourhood, std::stop_token aStop)
			-> std::expected<workspace_recovery_statistics, workspace_ownership_failure>
		{
			workspace_recovery_statistics      result;
			std::vector<std::filesystem::path> records;
			for (const auto& entry : std::filesystem::directory_iterator(aRegistry)) {
				if (aStop.stop_requested()) {
					return std::unexpected{ workspace_ownership_failure::cancelled };
				}
				if (entry.path().filename() == ".git-process-boundary.lock") {
					continue;
				}
				if (records.size() >= maximum_workspace_records) {
					return std::unexpected{ workspace_ownership_failure::capacity };
				}
				const auto name = entry.path().filename().string();
				if (name.size() != 38U || !name.ends_with(".jsonl") || !std::ranges::all_of(std::string_view{ name }.substr(0U, 32U), [](char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f'); })) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				records.push_back(entry.path());
			}
			// Names only identify artifacts that need repair; they never authorize
			// deletion. Missing journals cannot silently reset admission accounting.
			std::size_t inspected = 0U;
			for (const auto& entry : std::filesystem::directory_iterator(aNeighbourhood)) {
				if (aStop.stop_requested()) {
					return std::unexpected{ workspace_ownership_failure::cancelled };
				}
				if (++inspected > 4096U) {
					return std::unexpected{ workspace_ownership_failure::capacity };
				}
				const auto       name = entry.path().filename().string();
				std::string_view token{ name };
				if (token.starts_with("betterpullreview-agent-")) {
					token.remove_prefix(std::string_view{ "betterpullreview-agent-" }.size());
				} else if (token.starts_with(".bpr-cleanup-")) {
					token.remove_prefix(std::string_view{ ".bpr-cleanup-" }.size());
				} else {
					continue;
				}
				if (!std::ranges::any_of(records, [&](const auto& aPath) { return aPath.stem().string() == token; })) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
			}
			for (const auto& path : records) {
				if (aStop.stop_requested()) {
					return std::unexpected{ workspace_ownership_failure::cancelled };
				}
				if (!workspace_private_file(path)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				workspace_descriptor descriptor{ open_journal(path) };
				const auto           fd = descriptor.get();
				if (!descriptor.valid()) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				if (!journal_lock(fd, true)) {
					const auto busy = lock_busy();
					if (!busy) {
						return std::unexpected{ workspace_ownership_failure::unavailable };
					}
					++result.Live;
					continue;
				}
				auto record = load_workspace_record(fd, aRegistry, aNeighbourhood, path.stem().string());
				if (!record) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				workspace_ownership owner{ aRegistry, aNeighbourhood, std::move(*record), fd };
				static_cast<void>(descriptor.release());
				if (!owner.cleanup_locked()) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				++result.Recovered;
			}
			return result;
		}
#endif

		[[nodiscard]] auto parse_comparison_bases(
			const process_success&   aResult,
			const git_hash_algorithm aAlgorithm)
			-> git_process_result<application::git_comparison_base_observation>
		{
			return detail::parse_comparison_bases(aResult.ExitCode, aResult.StandardOutput, aAlgorithm);
		}

		class installed_git_process final
			: public application::git_process_port
		{
		public:
			installed_git_process(
				application::git_installation        aInstallation,
				std::filesystem::path                aIsolationRoot,
				std::unique_ptr<process_worker>      aWorker,
				std::optional<std::filesystem::path> aWorkspaceRegistry)
				: Installation{ std::move(aInstallation) }
				, IsolationRoot{ std::move(aIsolationRoot) }
				, RepositoriesRoot{ IsolationRoot / "repositories" / "v1" }
				, IncomingRoot{ IsolationRoot / "incoming" / "v1" }
				, TemplatesRoot{ IsolationRoot / "templates" }
				, HooksRoot{ IsolationRoot / "hooks" }
				, Worker{ std::move(aWorker) }
				, WorkspaceRegistry{ std::move(aWorkspaceRegistry) }
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
				return index_pack_impl(aRequest, aStop, false);
			}

			[[nodiscard]] auto index_sparse_pack(const application::git_index_pack_request& aRequest, std::stop_token aStop)
				-> git_process_result<application::git_index_pack_success> override
			{
				if (aRequest.ExpectedPack.Algorithm != application::git_hash_algorithm::sha1) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				return index_pack_impl(aRequest, aStop, true);
			}

			/// \brief The sparse path's fsck option for the bound installation.
			///
			/// \details `index-pack --fsck-objects` accepts
			/// `<msg-id>=<severity>` only from Git 2.44. Earlier installations
			/// reject the whole option, so they keep the bare form: the sparse
			/// import stays exactly as strict as it is today rather than failing
			/// on every pack.
			[[nodiscard]] auto sparse_fsck_argument() const -> std::string
			{
				if (
					Installation.VersionMajor < 2U
					|| (Installation.VersionMajor == 2U
						&& Installation.VersionMinor < 44U)) {
					return "--fsck-objects";
				}
				// [[BPR-0794]] Blobless packs also omit these special files. Git
				// otherwise requires .gitattributes and prints a missing
				// .gitmodules identity on stdout, refusing a valid sparse import.
				// Only absence is permitted here: present malformed objects keep
				// their error severity, and complete imports retain strict fsck.
				return "--fsck-objects=" + ignored_fsck_message_policy(Installation)
					+ ",gitattributesMissing=ignore,gitmodulesMissing=ignore";
			}

			[[nodiscard]] auto index_pack_impl(const application::git_index_pack_request& aRequest,
											   std::stop_token aStop, bool aSparse) -> git_process_result<application::git_index_pack_success>
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
				// [[BPR-0880]] Both paths carry the same closed list of Git's own
				// warning- and information-class message identifiers. `--strict`
				// has taken `<msg-id>=<severity>` since well before the oldest
				// admitted Git, because that is how `fetch-pack` has passed
				// `fetch.fsck.*` down since Git 2.19. `--fsck-objects` only
				// learned the same parameter in Git 2.44, so an older
				// installation keeps the bare option and the stricter behaviour
				// rather than being handed an argument it would reject outright.
				append_arguments(
					arguments,
					"index-pack",
					"--stdin",
					aSparse
						? sparse_fsck_argument()
						: "--strict=" + ignored_fsck_message_policy(Installation));
				if (!aSparse) {
					append_arguments(arguments, "--check-self-contained-and-connected");
				}
				append_arguments(arguments,
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
						// [[BPR-0846]] --check-self-contained-and-connected exits 1, after
						// writing the pack and reporting it on standard output, only when
						// every object the pack references resolves and some resolve to
						// objects already in the repository. A server that honours `have`
						// lines sends exactly that pack. A missing object or a broken
						// object still dies (128). Exit 1 proves only the pack's direct
						// references, so every caller verifies the full closure with
						// verify_connectivity before it publishes or journals anything.
						.AllowExitOne = !aSparse,
					},
					aStop);
				if (!result.has_value()) {
					// [[BPR-0880]] An error-class object is still refused, but it
					// is refused by name. `process_failure` alone told a reviewer
					// only that some Git command failed somewhere.
					if (result.error() == git_process_failure::nonzero_exit) {
						if (
							const auto refused = refused_fsck_message(
								result.error().Detail);
							!refused.empty()) {
							return std::unexpected{
								git_process_error{
									git_process_failure::unsupported_object,
									std::string{ refused } }
							};
						}
					}
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
				auto        parent          = canonical_existing(materialization.ParentDirectory);
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
				auto                                                 destination = (*parent / name).lexically_normal();
				auto                                                 identity    = std::optional<exact_artifact_identity>{ };
				std::unique_ptr<installed_agent_workspace_ownership> workspaceOwner;
				if (WorkspaceRegistry) {
					auto prepared = prepare_installed_agent_workspace_ownership(*WorkspaceRegistry, materialization, aStop);
					if (!prepared) {
						return std::unexpected{ git_process_error{
							prepared.error() == workspace_ownership_failure::cancelled ? git_process_failure::cancelled : git_process_failure::invalid_repository,
							"Disposable workspace ownership needs repair or has reached its limit." } };
					}
					workspaceOwner = std::move(*prepared);
					parent         = workspaceOwner->layout().Parent;
					destination    = workspaceOwner->layout().Workspace;
					identity       = exact_directory_identity(workspaceOwner->layout().WorkspaceIdentity);
					if (!identity || exact_identity(destination, exact_artifact_kind::directory) != identity || !empty_private_directory(destination)) {
						return std::unexpected{ git_process_failure::invalid_repository };
					}
				} else {
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
					identity = exact_identity(
						destination,
						exact_artifact_kind::directory);
					if (!identity.has_value()) {
						return std::unexpected{ git_process_failure::invalid_repository };
					}
				}
				auto       ownedPath = destination;
				const auto cleanup   = [&]() {
					if (workspaceOwner) {
						return workspaceOwner->cleanup();
					}
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

				const auto submitWorkspace = [&](process_request aProcess, const std::stop_token aToken) {
					if (workspaceOwner) {
						aProcess.WorkspaceGuard = workspaceOwner->layout().Guard;
					}
					return submit(std::move(aProcess), aToken);
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
				const auto initialized = submitWorkspace(
					process_request{
						.Arguments = std::move(initArguments),
						.Timeout   = pack_process_timeout,
					},
					aStop);
				if (!initialized.has_value()) {
					return fail(initialized.error());
				}

				// [[BPR-PENDING-AQ4]], RFC-BPR-0092 Q7. With a linked clone the
				// workspace borrows that clone's history read-only, through
				// Git's alternates list: one line naming the clone's objects
				// folder. Nothing is written to the clone, and the workspace has
				// no remote, so the agent's tools can read the history and blame
				// without reaching the reviewer's remotes.
				auto borrowed = std::optional<std::string>{ };
				if (materialization.HistoryClone) {
					const auto objects = checkout_objects(path_text(*materialization.HistoryClone));
					if (!objects) {
						return fail(git_process_error{ git_process_failure::invalid_request, "The linked clone has no objects folder this workspace can borrow history from." });
					}
					borrowed = path_text(*objects) + "\n";
					if (!write_private_file(destination / ".git" / "objects" / "info" / "alternates", *borrowed)) {
						return fail(git_process_failure::invalid_repository);
					}
				}
				// Whether the workspace already reads an object, through its
				// borrowed history or its own packs.
				const auto present = [&](const std::string& aObject) -> bool {
					auto probe = workspace_git_arguments(destination, HooksRoot);
					append_arguments(probe, "cat-file", "-e", aObject + "^{commit}");
					const auto answered = submitWorkspace(process_request{ .Arguments = std::move(probe), .AllowExitOne = true }, aStop);
					return answered.has_value() && answered->ExitCode == 0;
				};

				const auto packHashBytes = algorithm == git_hash_algorithm::sha1 ? 40U : 64U;
				auto       arguments     = std::vector<std::string>{ };
				auto       shallow       = false;
				const auto baseText      = std::string{ core::canonical_hex(materialization.Session.Binding.BaseOid).view() };
				if (borrowed && present(headText)) {
					// The clone already holds the head: nothing to copy.
				} else if (aRequest.Snapshot) {
					// [[BPR-1738]]. A sparse review's cache holds the head's
					// commit and trees but no blobs and no parents, so packing the
					// head's closure out of it walks into the first missing
					// parent. The snapshot carries exactly the head's commit, tree
					// and blobs; it is indexed into the workspace, which is then
					// shallow at the head, so nothing in it walks past the commit.
					const auto&     snapshot = *aRequest.Snapshot;
					std::error_code snapshotError;
					if (
						snapshot.Head != head || !snapshot.Pack.is_absolute() || snapshot.Bytes == 0U
						|| snapshot.Bytes > application::maximum_git_pack_input_bytes
						|| !std::filesystem::is_regular_file(snapshot.Pack, snapshotError) || snapshotError
						|| std::filesystem::file_size(snapshot.Pack, snapshotError) != snapshot.Bytes || snapshotError) {
						return fail(git_process_failure::invalid_request);
					}
					arguments = workspace_git_arguments(destination, HooksRoot);
					append_arguments(
						arguments,
						"index-pack",
						"--stdin",
						sparse_fsck_argument(),
						"--max-input-size=" + std::to_string(snapshot.Bytes),
						"--threads=1",
						"--no-rev-index");
					const auto indexedSnapshot = submitWorkspace(
						process_request{
							.Arguments = std::move(arguments),
							.Input     = file_input{
								.Path  = snapshot.Pack,
								.Bytes = snapshot.Bytes,
							},
							.Timeout = pack_process_timeout,
						},
						aStop);
					if (!indexedSnapshot.has_value()) {
						return fail(indexedSnapshot.error());
					}
					// Borrowed history ends the snapshot's boundary only when the
					// clone holds every parent of the head.
					auto parentsBorrowed = false;
					if (borrowed) {
						auto read = workspace_git_arguments(destination, HooksRoot);
						append_arguments(read, "cat-file", "commit", headText);
						const auto commitText = submitWorkspace(process_request{ .Arguments = std::move(read) }, aStop);
						if (commitText.has_value()) {
							auto parents = std::vector<std::string>{ };
							for (auto rest = std::string_view{ commitText->StandardOutput }; !rest.empty();) {
								const auto end  = rest.find('\n');
								const auto line = rest.substr(0U, end);
								if (line.empty()) {
									break;
								}
								if (line.starts_with("parent ")) {
									parents.emplace_back(line.substr(7U));
								}
								rest = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
							}
							parentsBorrowed = !parents.empty() && std::ranges::all_of(parents, present);
						}
					}
					if (!parentsBorrowed && !write_private_file(destination / ".git" / "shallow", headText + "\n")) {
						return fail(git_process_failure::invalid_repository);
					}
					shallow = !parentsBorrowed;
				} else {
					arguments = common_git_arguments(aRequest.Repository, HooksRoot);
					append_arguments(
						arguments,
						"pack-objects",
						"--revs",
						"--delta-base-offset",
						"--non-empty",
						"--",
						path_text(destination / ".git" / "objects" / "pack" / "pack"));
					// Borrowing a clone that holds the base, only what the base
					// lacks is copied; its history is read from the clone.
					const auto excludeBase = borrowed && baseText != headText && present(baseText);
					// [[BPR-PENDING-AQ5]]. Otherwise the base the cache holds is
					// copied too, so the brief's `git diff BASE...HEAD` works when
					// the base is not an ancestor of the head.
					auto cacheHasBase = false;
					if (!excludeBase && baseText != headText) {
						auto probe = common_git_arguments(aRequest.Repository, HooksRoot);
						append_arguments(probe, "cat-file", "-e", baseText + "^{commit}");
						const auto answered = submitWorkspace(process_request{ .Arguments = std::move(probe), .AllowExitOne = true }, aStop);
						cacheHasBase        = answered.has_value() && answered->ExitCode == 0;
					}
					auto tips = headText + "\n";
					if (excludeBase) {
						tips += "^" + baseText + "\n";
					} else if (cacheHasBase) {
						tips += baseText + "\n";
					}
					const auto packed = submitWorkspace(
						process_request{
							.Arguments = std::move(arguments),
							.Input     = memory_input{ std::move(tips) },
							.Timeout   = pack_process_timeout,
						},
						aStop);
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
				}

				arguments = workspace_git_arguments(destination, HooksRoot);
				append_arguments(arguments, "read-tree", "--reset", headText);
				const auto indexed = submitWorkspace(
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
				const auto materialized = submitWorkspace(
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
				const auto verifiedHead = submitWorkspace(
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
				const auto remotes = submitWorkspace(
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
				// The only alternates entry a workspace may hold is the one it
				// wrote for the linked clone.
				const auto alternatesFile = destination / ".git" / "objects" / "info" / "alternates";
				const auto unsafeTopology = (borrowed ? !read_exact_file(alternatesFile, *borrowed)
													  : std::filesystem::exists(alternatesFile, topologyError))
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
				if (workspaceOwner && !workspaceOwner->mark_ready()) {
					return fail(git_process_failure::invalid_repository);
				}
				auto owned = application::owned_agent_workspace{
					.Workspace = application::prepared_workspace{
						.Policy         = core::agent_workspace_policy::disposable_review_workspace,
						.PinnedHead     = head,
						.Path           = path_text(destination),
						.Cleanliness    = application::checkout_cleanliness::clean,
						.BorrowsHistory = borrowed.has_value(),
						.Shallow        = shallow,
					},
					.ApplicationSession = materialization.ApplicationSession,
					.Authority          = materialization.Authority,
					.Session            = materialization.Session.Id,
					.ParentDirectory    = *parent,
					.OwnershipMarker    = marker,
					.DirectoryIdentity  = workspace_directory_identity(*identity),
					.LifetimeGuard      = workspaceOwner ? std::optional{ workspaceOwner->layout().Guard } : std::nullopt,
				};
				if (workspaceOwner) {
					WorkspaceOwners.emplace(owned.LifetimeGuard->Path, std::make_pair(owned, std::move(workspaceOwner)));
				}
				return owned;
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
				if (WorkspaceRegistry) {
					if (!aWorkspace.LifetimeGuard) {
						return std::unexpected{ git_process_failure::invalid_request };
					}
					const auto found = WorkspaceOwners.find(aWorkspace.LifetimeGuard->Path);
					if (found == WorkspaceOwners.end() || found->second.first != aWorkspace) {
						return std::unexpected{ git_process_failure::invalid_request };
					}
					if (!found->second.second->cleanup()) {
						return std::unexpected{ git_process_failure::invalid_repository };
					}
					WorkspaceOwners.erase(found);
					return { };
				}
				if (aWorkspace.LifetimeGuard) {
					return std::unexpected{ git_process_failure::invalid_request };
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
				const auto parsedSize = detail::parse_blob_size(checked->StandardOutput, blob, aRequest.MaximumBytes);
				if (!parsedSize.has_value()) {
					return std::unexpected{ parsedSize.error() };
				}
				const auto size = *parsedSize;

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
				return detail::parse_ls_tree_blob(listed->StandardOutput, requestedPath, aRequest.Commit.Algorithm);
			}

			[[nodiscard]] auto changed_entries(const application::git_changed_entries_request& aRequest, application::git_changed_entry_sink& aSink, const std::stop_token aStop)
				-> git_process_result<void> override
			{
				if (!application::valid(aRequest.Left) || !application::valid(aRequest.Right) || aRequest.Left.Algorithm != aRequest.Right.Algorithm) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				if (!valid_repository(aRequest.Repository, aRequest.Left.Algorithm)) {
					return std::unexpected{ git_process_failure::invalid_repository };
				}
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(arguments, "diff-tree", "-r", "-z", "--raw", "--no-abbrev", "--no-commit-id", "--no-ext-diff", "--no-textconv", "--find-renames=100%", "--find-copies=100%", "-l10000", object_text(aRequest.Left), object_text(aRequest.Right), "--");
				const auto listed = submit(process_request{ .Arguments = std::move(arguments), .StandardOutputLimit = application::maximum_git_changed_catalogue_bytes }, aStop);
				if (!listed) {
					return std::unexpected{ listed.error() };
				}
				const auto parsed = detail::parse_changed_entries(listed->StandardOutput, aRequest.Left.Algorithm);
				if (!parsed) {
					return std::unexpected{ parsed.error() };
				}
				for (const auto& entry : *parsed) {
					if (aStop.stop_requested()) {
						return std::unexpected{ git_process_failure::cancelled };
					}
					if (!aSink.accept(entry)) {
						return std::unexpected{ git_process_failure::standard_output_limit };
					}
				}
				return { };
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

				return detail::parse_rename_pairs(
					renamed->StandardOutput,
					currentPath,
					previousPath.has_value() ? std::optional<std::string_view>{ *previousPath } : std::nullopt);
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

			[[nodiscard]] auto derive_comparison_base(
				const application::git_comparison_base_request& aRequest,
				const std::stop_token                           aStop)
				-> git_process_result<application::git_comparison_base_observation> override
			{
				if (!application::valid(aRequest.Head) || !application::valid(aRequest.SelectedBase)
					|| aRequest.Head.Algorithm != aRequest.SelectedBase.Algorithm) {
					return std::unexpected{ git_process_failure::invalid_request };
				}
				const auto unavailable = [](const git_process_error& aError)
					-> git_process_result<application::git_comparison_base_observation> {
					if (aError == git_process_failure::object_unavailable || aError == git_process_failure::nonzero_exit) {
						return application::git_comparison_base_observation{
							.Status = application::git_comparison_base_status::objects_unavailable,
						};
					}
					return std::unexpected{ aError };
				};
				const auto head = inspect_commit({ .Repository = aRequest.Repository, .Commit = aRequest.Head }, aStop);
				if (!head) {
					return unavailable(head.error());
				}
				const auto base = inspect_commit({ .Repository = aRequest.Repository, .Commit = aRequest.SelectedBase }, aStop);
				if (!base) {
					return unavailable(base.error());
				}
				// A merge-base walk may stop before a missing ancestor is visited.
				// Verify the full retained closure before claiming a unique result.
				const auto connected = verify_connectivity(
					{ .Repository = aRequest.Repository, .Base = aRequest.SelectedBase, .Head = aRequest.Head }, aStop);
				if (!connected) {
					return unavailable(connected.error());
				}
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(arguments, "merge-base", "--all", object_text(aRequest.Head), object_text(aRequest.SelectedBase));
				const auto result = submit(
					process_request{ .Arguments = std::move(arguments), .AllowExitOne = true }, aStop);
				if (!result) {
					return std::unexpected{ result.error() };
				}
				return parse_comparison_bases(*result, aRequest.Head.Algorithm);
			}

			[[nodiscard]] auto read_committer_time_seconds(
				const application::git_commit_request& aRequest,
				const std::stop_token                  aStop)
				-> git_process_result<std::optional<std::int64_t>> override
			{
				const auto inspected = inspect_commit(aRequest, aStop);
				if (!inspected) {
					if (inspected.error() == git_process_failure::object_unavailable) {
						return std::nullopt;
					}
					return std::unexpected{ inspected.error() };
				}
				// [[BPR-1340]]. `log --no-walk` reads the one commit and nothing
				// it names. `show --no-patch` still diffs the commit against its
				// parent, and a sparse acquisition stores each wanted head without
				// its ancestors, so on most pull requests the read failed and every
				// locally observed update said "Date unavailable".
				auto arguments = common_git_arguments(aRequest.Repository, HooksRoot);
				append_arguments(arguments, "log", "--no-walk", "--format=%ct", "--no-notes", "--no-show-signature", object_text(aRequest.Commit), "--");
				const auto result = submit(
					process_request{ .Arguments = std::move(arguments), .StandardOutputLimit = 32U }, aStop);
				if (!result) {
					return std::unexpected{ result.error() };
				}
				const auto seconds = detail::parse_committer_seconds(result->StandardOutput);
				if (!seconds.has_value()) {
					return std::unexpected{ seconds.error() };
				}
				return *seconds;
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
					process_request{
						.Arguments             = std::move(arguments),
						.SummarizeFailedOutput = summarize_connectivity_output,
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
				return detail::parse_ref_record(result->StandardOutput, *ref, *algorithm);
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

			application::git_installation                                                                                              Installation;
			std::filesystem::path                                                                                                      IsolationRoot;
			std::filesystem::path                                                                                                      RepositoriesRoot;
			std::filesystem::path                                                                                                      IncomingRoot;
			std::filesystem::path                                                                                                      TemplatesRoot;
			std::filesystem::path                                                                                                      HooksRoot;
			std::unique_ptr<process_worker>                                                                                            Worker;
			std::optional<std::filesystem::path>                                                                                       WorkspaceRegistry;
			std::map<std::string, std::pair<application::owned_agent_workspace, std::unique_ptr<installed_agent_workspace_ownership>>> WorkspaceOwners;
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

		using detail::parse_version;

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
				if (!missing(file) && (!exact_identity(file, exact_artifact_kind::regular_file) || !read_exact_file(file, contents))) {
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
				// Another Git command can be reading this shared policy. Adopting
				// its verified bytes must not truncate it or require a write handle.
				if (missing(file)) {
					const auto created = write_new_private_file(file, std::as_bytes(std::span{ contents }));
					if (created.Created && !created.Complete) {
						return false;
					}
				}
				if (!exact_identity(file, exact_artifact_kind::regular_file)
					|| !read_exact_file(file, contents)) {
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

		/// [[BPR-1172]]. The publication catalog's own store: one BPR-owned
		/// bare repository per object format. The bound checkout's objects are
		/// read through `GIT_ALTERNATE_OBJECT_DIRECTORIES` for one command at a
		/// time, so its configuration, hooks, refs and index are never
		/// consulted and nothing is written there; the store never records the
		/// checkout either.
		class installed_git_publication final
			: public application::publication_candidate_port
		{
		public:
			installed_git_publication(
				std::filesystem::path           aIsolationRoot,
				std::unique_ptr<process_worker> aWorker)
				: IsolationRoot{ std::move(aIsolationRoot) }
				, PublicationRoot{ IsolationRoot / "publication" / "v1" }
				, TemplatesRoot{ IsolationRoot / "templates" }
				, HooksRoot{ IsolationRoot / "hooks" }
				, Worker{ std::move(aWorker) }
			{
			}

			[[nodiscard]] auto list_commit(
				const application::publication_repository& aRepository,
				const core::git_commit_oid&                aCommit,
				const std::stop_token                      aStop)
				-> application::publication_git_result<application::publication_commit_listing> override
			{
				if (!aCommit.valid() || aCommit.algorithm() != aRepository.Algorithm) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				const auto context = prepare(aRepository, aStop);
				if (!context) {
					return std::unexpected{ context.error() };
				}
				const auto commit = std::string{ core::canonical_hex(aCommit).view() };
				auto       type   = run(*context, { "cat-file", "-t", commit }, standard_output_limit, aStop);
				if (!type) {
					return std::unexpected{ missing_or(type.error()) };
				}
				if (*type != "commit\n") {
					return failure(application::publication_git_failure::object_unavailable);
				}
				auto object = run(*context, { "cat-file", "commit", commit }, maximum_commit_object_bytes, aStop);
				if (!object) {
					return std::unexpected{ missing_or(object.error()) };
				}
				const auto text   = std::string_view{ *object };
				const auto length = aCommit.byte_count() * 2U;
				if (!text.starts_with("tree ") || text.size() < 6U + length || text[5U + length] != '\n') {
					return failure(application::publication_git_failure::malformed);
				}
				const auto tree = core::parse_git_tree_oid(text.substr(5U, length));
				if (!tree || tree->algorithm() != aCommit.algorithm()) {
					return failure(application::publication_git_failure::malformed);
				}
				auto entries = list_tree(*context, *tree, aStop);
				if (!entries) {
					return std::unexpected{ entries.error() };
				}
				return application::publication_commit_listing{
					.CommitObject = std::vector<std::uint8_t>(object->begin(), object->end()),
					.Tree         = *tree,
					.Entries      = std::move(*entries),
				};
			}

			[[nodiscard]] auto write_blob(
				const application::publication_repository& aRepository,
				const core::git_entry_oid&                 aExpected,
				const std::span<const std::uint8_t>        aBytes,
				const std::stop_token                      aStop)
				-> application::publication_git_result<void> override
			{
				if (!aExpected.valid() || aExpected.algorithm() != aRepository.Algorithm) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				const auto context = prepare(aRepository, aStop);
				if (!context) {
					return std::unexpected{ context.error() };
				}
				auto stored = run(*context, { "hash-object", "-t", "blob", "-w", "--no-filters", "--stdin" }, standard_output_limit, aStop,
								  std::string{ reinterpret_cast<const char*>(aBytes.data()), aBytes.size() });
				if (!stored) {
					return std::unexpected{ stored.error() };
				}
				return matches(*stored, core::canonical_hex(aExpected).view());
			}

			[[nodiscard]] auto write_tree(
				const application::publication_repository&   aRepository,
				const application::publication_tree_request& aRequest,
				const std::stop_token                        aStop)
				-> application::publication_git_result<void> override
			{
				const auto algorithm = aRepository.Algorithm;
				if (!aRequest.Base.valid() || aRequest.Base.algorithm() != algorithm || !aRequest.Expected.valid() || aRequest.Expected.algorithm() != algorithm
					|| aRequest.Changes.size() + aRequest.Deletions.size() > core::maximum_git_tree_entries) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				auto records = std::string{ };
				for (const auto& change : aRequest.Changes) {
					if (!change.Object.valid() || change.Object.algorithm() != algorithm || !core::valid_git_entry_mode(change.Mode) || change.Path.empty()
						|| change.Path.find('\0') != std::string::npos) {
						return failure(application::publication_git_failure::invalid_repository);
					}
					records.append(octal(change.Mode)).append(" ").append(core::canonical_hex(change.Object).view()).append("\t").append(change.Path);
					records.push_back('\0');
				}
				const auto zero = std::string(algorithm == core::git_hash_algorithm::sha1 ? 40U : 64U, '0');
				for (const auto& deletion : aRequest.Deletions) {
					if (deletion.empty() || deletion.find('\0') != std::string::npos) {
						return failure(application::publication_git_failure::invalid_repository);
					}
					records.append("0 ").append(zero).append("\t").append(deletion);
					records.push_back('\0');
				}
				auto context = prepare(aRepository, aStop);
				if (!context) {
					return std::unexpected{ context.error() };
				}
				// One private index at a time, shared by no checkout.
				auto boundary = process_boundary_lock{ IsolationRoot, aStop };
				if (!boundary.valid()) {
					return failure(aStop.stop_requested() ? application::publication_git_failure::cancelled : application::publication_git_failure::unavailable);
				}
				const auto index = IsolationRoot / "tmp" / "publication-index";
				auto       error = std::error_code{ };
				std::filesystem::remove(index, error);
				if (error) {
					return failure(application::publication_git_failure::unavailable);
				}
				context->Index     = path_text(index);
				const auto cleanup = [&index] {
					auto ignored = std::error_code{ };
					std::filesystem::remove(index, ignored);
				};
				const auto base = run(*context, { "read-tree", core::canonical_hex(aRequest.Base).view() }, standard_output_limit, aStop);
				if (!base) {
					cleanup();
					return std::unexpected{ missing_or(base.error()) };
				}
				if (!records.empty()) {
					const auto updated = run(*context, { "update-index", "-z", "--index-info" }, standard_output_limit, aStop, std::move(records));
					if (!updated) {
						cleanup();
						return std::unexpected{ updated.error() };
					}
				}
				const auto written = run(*context, { "write-tree" }, standard_output_limit, aStop);
				cleanup();
				if (!written) {
					return std::unexpected{ missing_or(written.error()) };
				}
				return matches(*written, core::canonical_hex(aRequest.Expected).view());
			}

			[[nodiscard]] auto write_commit(
				const application::publication_repository& aRepository,
				const core::git_commit_content&            aCommit,
				const core::git_commit_oid&                aExpected,
				const std::stop_token                      aStop)
				-> application::publication_git_result<void> override
			{
				if (!aExpected.valid() || aExpected.algorithm() != aRepository.Algorithm || aCommit.Tree.algorithm() != aRepository.Algorithm
					|| !core::encode_git_commit(aCommit).has_value()) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				auto context = prepare(aRepository, aStop);
				if (!context) {
					return std::unexpected{ context.error() };
				}
				context->Signatures = {
					{ "GIT_AUTHOR_NAME", aCommit.Author.Name },
					{ "GIT_AUTHOR_EMAIL", aCommit.Author.Email },
					{ "GIT_AUTHOR_DATE", git_date(aCommit.Author) },
					{ "GIT_COMMITTER_NAME", aCommit.Committer.Name },
					{ "GIT_COMMITTER_EMAIL", aCommit.Committer.Email },
					{ "GIT_COMMITTER_DATE", git_date(aCommit.Committer) },
				};
				auto arguments = std::vector<std::string>{ "commit-tree", "--no-gpg-sign", std::string{ core::canonical_hex(aCommit.Tree).view() } };
				for (const auto& parent : aCommit.Parents) {
					arguments.emplace_back("-p");
					arguments.emplace_back(core::canonical_hex(parent).view());
				}
				const auto written = run_arguments(*context, std::move(arguments), standard_output_limit, aStop, aCommit.Message);
				if (!written) {
					return std::unexpected{ missing_or(written.error()) };
				}
				return matches(*written, core::canonical_hex(aExpected).view());
			}

			[[nodiscard]] auto diff_trees(
				const application::publication_repository& aRepository,
				const core::git_tree_oid&                  aFrom,
				const core::git_tree_oid&                  aTo,
				const std::stop_token                      aStop)
				-> application::publication_git_result<std::vector<std::uint8_t>> override
			{
				if (!aFrom.valid() || !aTo.valid() || aFrom.algorithm() != aRepository.Algorithm || aTo.algorithm() != aRepository.Algorithm) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				const auto context = prepare(aRepository, aStop);
				if (!context) {
					return std::unexpected{ context.error() };
				}
				auto patch = run(*context,
								 { "diff-tree", "-r", "-p", "--binary", "--full-index", "--no-ext-diff", "--no-textconv", "--no-renames", "--no-color", core::canonical_hex(aFrom).view(),
								   core::canonical_hex(aTo).view() },
								 application::maximum_publication_patch_bytes, aStop);
				if (!patch) {
					return std::unexpected{ missing_or(patch.error()) };
				}
				return std::vector<std::uint8_t>(patch->begin(), patch->end());
			}

		private:
			static constexpr std::size_t maximum_commit_object_bytes = 1024U * 1024U;
			static constexpr std::size_t maximum_listing_bytes       = 64U * 1024U * 1024U;

			struct command_context
			{
				std::filesystem::path Store;
				std::string           SourceObjects;
				/// A private index for this command, when one is in use.
				std::string Index;
				/// Author and committer fields for `commit-tree`.
				std::vector<std::pair<std::string, std::string>> Signatures;
			};

			[[nodiscard]] static auto octal(const std::uint32_t aMode) -> std::string
			{
				auto text  = std::string{ };
				auto value = aMode;
				do {
					text.insert(text.begin(), static_cast<char>('0' + (value & 7U)));
					value >>= 3U;
				} while (value != 0U);
				return text;
			}

			/// Git's raw date: seconds and a signed hhmm offset.
			[[nodiscard]] static auto git_date(const core::git_signature& aSignature) -> std::string
			{
				const auto minutes = aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes;
				auto       text    = std::to_string(aSignature.Seconds) + (aSignature.OffsetMinutes < 0 ? " -" : " +");
				text.push_back(static_cast<char>('0' + minutes / 600));
				text.push_back(static_cast<char>('0' + (minutes / 60) % 10));
				text.push_back(static_cast<char>('0' + (minutes % 60) / 10));
				text.push_back(static_cast<char>('0' + minutes % 10));
				return text;
			}

			/// Git's one-line answer must be exactly the expected identity.
			[[nodiscard]] static auto matches(const std::string_view aOutput, const std::string_view aExpected) -> application::publication_git_result<void>
			{
				if (aOutput.size() != aExpected.size() + 1U || aOutput.back() != '\n') {
					return failure(application::publication_git_failure::malformed);
				}
				if (aOutput.substr(0U, aExpected.size()) != aExpected) {
					return failure(application::publication_git_failure::identity_mismatch);
				}
				return { };
			}

			[[nodiscard]] static auto failure(
				const application::publication_git_failure aFailure,
				std::string                                aDetail = { })
				-> std::unexpected<application::publication_git_error>
			{
				return std::unexpected{ application::publication_git_error{ .Failure = aFailure, .Detail = std::move(aDetail) } };
			}

			[[nodiscard]] static auto translate(const git_process_error& aError) -> application::publication_git_error
			{
				switch (aError.Code) {
				case git_process_failure::cancelled:
					return { .Failure = application::publication_git_failure::cancelled };
				case git_process_failure::standard_output_limit:
					return { .Failure = application::publication_git_failure::bounds };
				case git_process_failure::object_unavailable:
					return { .Failure = application::publication_git_failure::object_unavailable, .Detail = aError.Detail };
				case git_process_failure::malformed_output:
					return { .Failure = application::publication_git_failure::malformed, .Detail = aError.Detail };
				default:
					return { .Failure = application::publication_git_failure::unavailable, .Detail = aError.Detail };
				}
			}

			/// A command that failed on a missing object says so; anything
			/// else keeps its own category.
			[[nodiscard]] static auto missing_or(const application::publication_git_error& aError) -> application::publication_git_error
			{
				if (aError.Failure == application::publication_git_failure::unavailable && aError.Detail.find("ot a valid object name") != std::string::npos) {
					return { .Failure = application::publication_git_failure::object_unavailable, .Detail = aError.Detail };
				}
				return aError;
			}

			[[nodiscard]] auto run(
				const command_context&                        aContext,
				const std::initializer_list<std::string_view> aArguments,
				const std::size_t                             aOutputLimit,
				const std::stop_token                         aStop,
				std::string                                   aInput = { })
				-> application::publication_git_result<std::string>
			{
				auto arguments = std::vector<std::string>{ };
				for (const auto argument : aArguments) {
					arguments.emplace_back(argument);
				}
				return run_arguments(aContext, std::move(arguments), aOutputLimit, aStop, std::move(aInput));
			}

			/// Runs one plumbing command in the private store with the
			/// checkout's objects as a read-only alternate.
			[[nodiscard]] auto run_arguments(
				const command_context&   aContext,
				std::vector<std::string> aArguments,
				const std::size_t        aOutputLimit,
				const std::stop_token    aStop,
				std::string              aInput = { })
				-> application::publication_git_result<std::string>
			{
				auto arguments = common_git_arguments(aContext.Store, HooksRoot);
				std::ranges::move(aArguments, std::back_inserter(arguments));
				if (!empty_private_directory(TemplatesRoot) || !empty_private_directory(HooksRoot)) {
					return failure(application::publication_git_failure::unavailable);
				}
				auto environment = std::vector<std::pair<std::string, std::string>>{ { "GIT_ALTERNATE_OBJECT_DIRECTORIES", aContext.SourceObjects } };
				if (!aContext.Index.empty()) {
					environment.emplace_back("GIT_INDEX_FILE", aContext.Index);
				}
				environment.insert(environment.end(), aContext.Signatures.begin(), aContext.Signatures.end());
				auto result = Worker->submit(
					process_request{
						.Arguments           = std::move(arguments),
						.Input               = memory_input{ std::move(aInput) },
						.Timeout             = pack_process_timeout,
						.StandardOutputLimit = aOutputLimit,
						.Environment         = std::move(environment),
					},
					aStop);
				if (!result) {
					return std::unexpected{ translate(result.error()) };
				}
				return std::move(result->StandardOutput);
			}

			[[nodiscard]] auto prepare(
				const application::publication_repository& aRepository,
				const std::stop_token                      aStop)
				-> application::publication_git_result<command_context>
			{
				if (aRepository.Algorithm != core::git_hash_algorithm::sha1 && aRepository.Algorithm != core::git_hash_algorithm::sha256) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				const auto objects = checkout_objects(aRepository.Checkout);
				if (!objects) {
					return failure(application::publication_git_failure::invalid_repository);
				}
				const auto algorithm = aRepository.Algorithm == core::git_hash_algorithm::sha1 ? git_hash_algorithm::sha1 : git_hash_algorithm::sha256;
				const auto store     = PublicationRoot / (algorithm == git_hash_algorithm::sha1 ? "sha1.git" : "sha256.git");
				if (!valid_store(store, algorithm)) {
					auto boundary = process_boundary_lock{ IsolationRoot, aStop };
					if (!boundary.valid()) {
						return failure(aStop.stop_requested() ? application::publication_git_failure::cancelled : application::publication_git_failure::unavailable);
					}
					if (!valid_store(store, algorithm) && !initialize_store(store, algorithm, aStop)) {
						return failure(aStop.stop_requested() ? application::publication_git_failure::cancelled : application::publication_git_failure::unavailable);
					}
				}
				return command_context{ .Store = store, .SourceObjects = path_text(*objects) };
			}

			[[nodiscard]] auto valid_store(const std::filesystem::path& aStore, const git_hash_algorithm aAlgorithm) const -> bool
			{
				std::error_code error;
				return private_directory(aStore) && private_directory(aStore / "objects") && private_directory(aStore / "refs")
					&& read_exact_file(aStore / "config", repository_config(aAlgorithm)) && read_exact_file(aStore / "HEAD", "ref: refs/heads/main\n")
					&& !std::filesystem::exists(aStore / "objects" / "info" / "alternates", error) && !error
					&& !std::filesystem::exists(aStore / "objects" / "info" / "http-alternates", error) && !error
					&& !std::filesystem::exists(aStore / "info" / "grafts", error) && !error && !std::filesystem::exists(aStore / "shallow", error) && !error;
			}

			[[nodiscard]] auto initialize_store(const std::filesystem::path& aStore, const git_hash_algorithm aAlgorithm, const std::stop_token aStop) -> bool
			{
				if (!make_private_directory(IsolationRoot / "publication") || !make_private_directory(PublicationRoot)) {
					return false;
				}
				std::error_code error;
				if (std::filesystem::exists(aStore, error) || error || !make_private_directory(aStore)) {
					return false;
				}
				const auto initialized = Worker->submit(
					process_request{
						.Arguments = {
							"init",
							"--bare",
							"--quiet",
							"--initial-branch=main",
							aAlgorithm == git_hash_algorithm::sha1 ? "--object-format=sha1" : "--object-format=sha256",
							"--template=" + path_text(TemplatesRoot),
							path_text(aStore),
						},
						.Input   = memory_input{ },
						.Timeout = process_timeout,
					},
					aStop);
				return initialized.has_value() && initialized->StandardOutput.empty() && write_private_file(aStore / "config", repository_config(aAlgorithm))
					&& write_private_file(aStore / "HEAD", "ref: refs/heads/main\n") && valid_store(aStore, aAlgorithm);
			}

			[[nodiscard]] auto list_tree(
				const command_context&    aContext,
				const core::git_tree_oid& aTree,
				const std::stop_token     aStop)
				-> application::publication_git_result<std::vector<core::git_tree_entry>>
			{
				const auto tree   = std::string{ core::canonical_hex(aTree).view() };
				auto       output = run(aContext, { "ls-tree", "-r", "-z", "--full-tree", tree }, maximum_listing_bytes, aStop);
				if (!output) {
					return std::unexpected{ missing_or(output.error()) };
				}
				auto entries = std::vector<core::git_tree_entry>{ };
				auto rest    = std::string_view{ *output };
				while (!rest.empty()) {
					const auto end = rest.find('\0');
					if (end == std::string_view::npos) {
						return failure(application::publication_git_failure::malformed);
					}
					const auto record = rest.substr(0U, end);
					rest.remove_prefix(end + 1U);
					const auto tab    = record.find('\t');
					const auto first  = record.find(' ');
					const auto second = first == std::string_view::npos ? std::string_view::npos : record.find(' ', first + 1U);
					if (tab == std::string_view::npos || second == std::string_view::npos || second > tab) {
						return failure(application::publication_git_failure::malformed);
					}
					const auto modeText = record.substr(0U, first);
					const auto type     = record.substr(first + 1U, second - first - 1U);
					const auto object   = core::parse_git_entry_oid(record.substr(second + 1U, tab - second - 1U));
					auto       mode     = std::uint32_t{ 0 };
					const auto parsed   = std::from_chars(modeText.data(), modeText.data() + modeText.size(), mode, 8);
					if (parsed.ec != std::errc{ } || parsed.ptr != modeText.data() + modeText.size() || !object || object->algorithm() != aTree.algorithm()
						|| (type != "blob" && type != "commit") || (type == "commit") != (mode == core::git_mode_gitlink)) {
						return failure(application::publication_git_failure::malformed);
					}
					if (entries.size() == core::maximum_git_tree_entries) {
						return failure(application::publication_git_failure::bounds);
					}
					entries.push_back({ .Path = std::string{ record.substr(tab + 1U) }, .Mode = mode, .Object = *object });
				}
				return entries;
			}

			std::filesystem::path           IsolationRoot;
			std::filesystem::path           PublicationRoot;
			std::filesystem::path           TemplatesRoot;
			std::filesystem::path           HooksRoot;
			std::unique_ptr<process_worker> Worker;
		};

		struct bootstrapped_git
		{
			application::git_installation   Installation;
			std::filesystem::path           Isolation;
			std::unique_ptr<process_worker> Worker;
		};

		/// Prepares the isolation root, resolves the installed Git through its
		/// own exec path and admits its version: what every installed-Git
		/// catalog needs before its first command.
		[[nodiscard]] auto bootstrap_installed_git(
			const std::filesystem::path&                    aGitExecutable,
			const std::filesystem::path&                    aIsolationRoot,
			const std::optional<std::filesystem::path>&     aWindowsSystemRoot,
			const std::stop_token                           aStop,
			std::shared_ptr<application::insights_observer> aInsights = { })
			-> std::expected<bootstrapped_git, application::git_process_error>
		{
			const auto isolation = canonical_existing(aIsolationRoot);
			const auto candidate = canonical_existing(aGitExecutable);
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
			const auto systemRoot = aWindowsSystemRoot.has_value()
				? canonical_existing(*aWindowsSystemRoot)
				: std::nullopt;
			if (!systemRoot.has_value()) {
				return std::unexpected{
					application::git_process_failure::
						invalid_configuration
				};
			}
#else
			static_cast<void>(aWindowsSystemRoot);
			const auto systemRoot = std::optional<std::filesystem::path>{ };
#endif

			auto bootstrapConfig = runner_config(
				*candidate,
				*isolation,
				std::nullopt,
				systemRoot);
			bootstrapConfig.Insights  = aInsights;
			auto       bootstrap      = process_worker{ std::move(bootstrapConfig) };
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

			auto workerConfig = runner_config(
				*resolvedGit,
				*isolation,
				execPath,
				systemRoot);
			workerConfig.Insights       = aInsights;
			auto       worker           = std::make_unique<process_worker>(std::move(workerConfig));
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

			return bootstrapped_git{
				.Installation = {
					.VersionMajor = (*version)[0],
					.VersionMinor = (*version)[1],
					.VersionPatch = (*version)[2],
				},
				.Isolation = *isolation,
				.Worker    = std::move(worker),
			};
		}
	}

	auto recover_installed_agent_workspaces(
		const std::filesystem::path& aRegistry,
		const std::filesystem::path& aNeighbourhood,
		const std::stop_token        aStop)
		-> std::expected<workspace_recovery_statistics, workspace_ownership_failure>
	{
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		try {
			if (aStop.stop_requested()) {
				return std::unexpected{ workspace_ownership_failure::cancelled };
			}
			if (!prepare_workspace_registry(aRegistry, aNeighbourhood)) {
				return std::unexpected{ workspace_ownership_failure::repair_required };
			}
			auto boundary = process_boundary_lock{ aRegistry, aStop };
			if (!boundary.valid()) {
				return std::unexpected{ aStop.stop_requested() ? workspace_ownership_failure::cancelled : workspace_ownership_failure::unavailable };
			}
			return recover_workspaces_locked(aRegistry, aNeighbourhood, aStop);
		} catch (...) {
			return std::unexpected{ workspace_ownership_failure::unavailable };
		}
#else
		static_cast<void>(aRegistry);
		static_cast<void>(aNeighbourhood);
		static_cast<void>(aStop);
		return std::unexpected{ workspace_ownership_failure::unavailable };
#endif
	}

	auto prepare_installed_agent_workspace_ownership(
		const std::filesystem::path&                                aRegistry,
		const application::agent_workspace_materialization_request& aRequest,
		const std::stop_token                                       aStop)
		-> std::expected<std::unique_ptr<installed_agent_workspace_ownership>, workspace_ownership_failure>
	{
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		try {
			if (aStop.stop_requested()) {
				return std::unexpected{ workspace_ownership_failure::cancelled };
			}
			if (!application::valid(aRequest) || !prepare_workspace_registry(aRegistry, aRequest.ParentDirectory)) {
				return std::unexpected{ workspace_ownership_failure::repair_required };
			}
			auto boundary = process_boundary_lock{ aRegistry, aStop };
			if (!boundary.valid()) {
				return std::unexpected{ aStop.stop_requested() ? workspace_ownership_failure::cancelled : workspace_ownership_failure::unavailable };
			}
			const auto recovered = recover_workspaces_locked(aRegistry, aRequest.ParentDirectory, aStop);
			if (!recovered) {
				return std::unexpected{ recovered.error() };
			}
			if (recovered->Live >= maximum_workspace_records) {
				return std::unexpected{ workspace_ownership_failure::capacity };
			}
			std::random_device random;
			for (std::size_t attempt = 0U; attempt < 8U; ++attempt) {
				if (aStop.stop_requested()) {
					return std::unexpected{ workspace_ownership_failure::cancelled };
				}
				std::array<std::uint8_t, 16U> nonce{ };
				for (auto& byte : nonce) {
					byte = static_cast<std::uint8_t>(random());
				}
				workspace_record record{
					.Token         = lower_hex(nonce),
					.Registry      = ownership_id(aRegistry, exact_artifact_kind::directory),
					.Neighbourhood = ownership_id(aRequest.ParentDirectory, exact_artifact_kind::directory),
					.Scope         = { aRequest.ApplicationSession.value(), aRequest.Authority.Host.value(), aRequest.Authority.Repository.value(), aRequest.Authority.PullRequest.value(), aRequest.Session.Id.value(), aRequest.Authority.Profile.value(), aRequest.Authority.ProfileRevision.value(), aRequest.Authority.Account.value(), aRequest.Authority.Viewer.value(), aRequest.Authority.Number.value() },
					.Head          = std::string{ core::canonical_hex(aRequest.Session.Binding.HeadOid).view() },
				};
				std::ranges::copy(aRequest.Authority.HostAuthority.bytes(), record.AuthorityFingerprints[0].begin());
				std::ranges::copy(aRequest.Authority.Cache.bytes(), record.AuthorityFingerprints[1].begin());
				std::ranges::copy(aRequest.Authority.Scope.bytes(), record.AuthorityFingerprints[2].begin());
				const auto journal = aRegistry / (record.Token + ".jsonl");
				const auto parent  = aRequest.ParentDirectory / ("betterpullreview-agent-" + record.Token);
				if (!absent(parent) || !absent(aRequest.ParentDirectory / (".bpr-cleanup-" + record.Token))) {
					continue;
				}
				auto                 created = journal_invalid;
				const auto           outcome = create_journal(journal, created);
				workspace_descriptor descriptor{ created };
				if (outcome == journal_creation::exists) {
					continue;
				}
				if (outcome != journal_creation::created) {
					return std::unexpected{ workspace_ownership_failure::unavailable };
				}
				const auto status = journal_stat(descriptor.get());
				if (!status || !status->Private || !journal_lock(descriptor.get(), false)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				record.File = status->File;
				auto owner  = std::make_unique<workspace_ownership>(aRegistry, aRequest.ParentDirectory, record, descriptor.get());
				static_cast<void>(descriptor.release());
				if (!owner->append(record, true) || !sync_workspace_directory(aRegistry)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				// Exclusive mkdir never adopts a pre-existing directory. A crash
				// before recording its identity preserves it and refuses admission.
				if (!create_workspace_directory(parent) || !sync_workspace_directory(aRequest.ParentDirectory)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				workspace_checkpoint("parent-created");
				record.Parent = ownership_id(parent, exact_artifact_kind::directory);
				record.Phase  = 1U;
				if (!owner->append(record)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				if (!create_workspace_directory(owner->layout().Workspace) || !sync_workspace_directory(parent)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				workspace_checkpoint("child-created");
				record.Child = ownership_id(owner->layout().Workspace, exact_artifact_kind::directory);
				record.Phase = 2U;
				if (!owner->append(record)) {
					return std::unexpected{ workspace_ownership_failure::repair_required };
				}
				return std::unique_ptr<installed_agent_workspace_ownership>{ std::move(owner) };
			}
			return std::unexpected{ workspace_ownership_failure::capacity };
		} catch (...) {
			return std::unexpected{ workspace_ownership_failure::unavailable };
		}
#else
		static_cast<void>(aRegistry);
		static_cast<void>(aRequest);
		static_cast<void>(aStop);
		return std::unexpected{ workspace_ownership_failure::unavailable };
#endif
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
			auto bootstrapped = bootstrap_installed_git(aConfiguration.GitExecutable, aConfiguration.IsolationRoot, aConfiguration.WindowsSystemRoot, aStop, aConfiguration.Insights);
			if (!bootstrapped) {
				return std::unexpected{ std::move(bootstrapped.error()) };
			}
			auto implementation = std::make_unique<installed_git_process>(
				bootstrapped->Installation,
				bootstrapped->Isolation,
				std::move(bootstrapped->Worker), aConfiguration.WorkspaceOwnershipRegistry);
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

	auto make_installed_git_publication(
		const installed_git_publication_configuration& aConfiguration,
		const std::stop_token                          aStop)
		-> std::expected<
			std::unique_ptr<application::publication_candidate_port>,
			application::git_process_error>
	{
		try {
			if (aStop.stop_requested()) {
				return std::unexpected{ application::git_process_failure::cancelled };
			}
			auto bootstrapped = bootstrap_installed_git(aConfiguration.GitExecutable, aConfiguration.IsolationRoot, aConfiguration.WindowsSystemRoot, aStop);
			if (!bootstrapped) {
				return std::unexpected{ std::move(bootstrapped.error()) };
			}
			return std::unique_ptr<application::publication_candidate_port>{
				std::make_unique<installed_git_publication>(bootstrapped->Isolation, std::move(bootstrapped->Worker))
			};
		} catch (...) {
			return std::unexpected{ application::git_process_failure::invalid_configuration };
		}
	}
}
