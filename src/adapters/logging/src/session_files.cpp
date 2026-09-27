#include "session_files.hpp"
#include "../../native/windows_private_storage.hpp"
#include "bpr/application/diagnostic_log_files.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <limits>
#include <thread>
#include <utility>
#include <vector>
#if !defined(_WIN32)
	#include <dirent.h>
	#include <fcntl.h>
	#include <sys/file.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif
namespace bpr::adapters::logging {
	namespace {
		using status  = application::diagnostic_log_status;
		using failure = diagnostic_file_failure;
		struct storage_error final : std::exception
		{
			failure Reason;
			explicit storage_error(failure aReason)
				: Reason{ aReason }
			{ }
			auto what() const noexcept -> const char* override { return "diagnostic storage refused"; }
		};
		[[noreturn]] void          fail(failure aFailure) { throw storage_error{ aFailure }; }
		constexpr std::string_view lock_name = ".retention.lock";
		struct file_info
		{
			std::uint64_t Bytes    = 0U;
			std::int64_t  Modified = 0;
#if !defined(_WIN32)
			dev_t Device{ };
			ino_t Inode{ };
#else
			native_storage::file_identity Identity;
#endif
			auto operator==(const file_info&) const -> bool = default;
		};
#if !defined(_WIN32)
		struct handle
		{
			int Value = -1;
			explicit handle(int aValue = -1)
				: Value{ aValue }
			{ }
			~handle()
			{
				if (Value >= 0) {
					::close(Value);
				}
			}
			handle(handle&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{ }
			auto operator=(handle&& aOther) noexcept -> handle&
			{
				std::swap(Value, aOther.Value);
				return *this;
			}
			[[nodiscard]] auto valid() const -> bool { return Value >= 0; }
		};
		auto information(const handle& aFile) -> file_info
		{
			struct stat info{ };
			if (::fstat(aFile.Value, &info) != 0) {
				fail(failure::io_failure);
			}
			if (!S_ISREG(info.st_mode) || info.st_uid != ::geteuid() || info.st_nlink != 1 || (info.st_mode & 0077U) != 0 || info.st_size < 0) {
				fail(failure::invalid_permissions);
			}
			return { static_cast<std::uint64_t>(info.st_size), static_cast<std::int64_t>(info.st_mtime), info.st_dev, info.st_ino };
		}
		auto lock(const handle& aFile) -> bool
		{
			if (::flock(aFile.Value, LOCK_EX | LOCK_NB) == 0) {
				return true;
			}
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				fail(failure::io_failure);
			}
			return false;
		}
		void unlock(const handle& aFile) noexcept { static_cast<void>(::flock(aFile.Value, LOCK_UN)); }
		class directory
		{
		public:
			explicit directory(const std::filesystem::path& aRoot)
				: RootPath{ aRoot }
			{
				if (!aRoot.is_absolute() || aRoot.lexically_normal() != aRoot) {
					fail(failure::invalid_root);
				}
				struct stat named{ }, admitted{ };
				if (::lstat(aRoot.c_str(), &named) != 0 || !S_ISDIR(named.st_mode)) {
					fail(failure::invalid_root);
				}
				Root = handle{ ::open(aRoot.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (!Root.valid() || ::fstat(Root.Value, &admitted) != 0 || admitted.st_dev != named.st_dev || admitted.st_ino != named.st_ino) {
					fail(failure::invalid_root);
				}
				if (admitted.st_uid != ::geteuid() || (admitted.st_mode & 0077U) != 0) {
					fail(failure::invalid_permissions);
				}
				if (::mkdirat(Root.Value, "logs", 0700) != 0 && errno != EEXIST) {
					fail(failure::io_failure);
				}
				Directory = handle{ ::openat(Root.Value, "logs", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (!Directory.valid() || ::fstat(Directory.Value, &admitted) != 0 || admitted.st_uid != ::geteuid() || (admitted.st_mode & 0077U) != 0) {
					fail(failure::invalid_permissions);
				}
			}
			auto open(const std::string& aName, bool aCreate, bool aExclusive = false) const -> handle
			{
				handle file{ ::openat(Directory.Value, aName.c_str(), O_RDWR | O_APPEND | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | (aCreate ? O_CREAT : 0) | (aExclusive ? O_EXCL : 0), 0600) };
				if (!file.valid()) {
					fail(errno == ELOOP ? failure::invalid_permissions : failure::io_failure);
				}
				static_cast<void>(information(file));
				verify(file, aName);
				return file;
			}
			void verify(const handle& aFile, const std::string& aName) const
			{
				const auto  held = information(aFile);
				struct stat named{ }, rootInfo{ }, rootName{ }, directoryInfo{ }, directoryName{ };
				if (::fstat(Root.Value, &rootInfo) != 0 || ::lstat(RootPath.c_str(), &rootName) != 0 || !S_ISDIR(rootName.st_mode) || rootName.st_dev != rootInfo.st_dev || rootName.st_ino != rootInfo.st_ino || rootInfo.st_uid != ::geteuid() || (rootInfo.st_mode & 0077U) != 0) {
					fail(failure::invalid_permissions);
				}
				if (::fstat(Directory.Value, &directoryInfo) != 0 || ::fstatat(Root.Value, "logs", &directoryName, AT_SYMLINK_NOFOLLOW) != 0
					|| !S_ISDIR(directoryName.st_mode) || directoryInfo.st_uid != ::geteuid() || (directoryInfo.st_mode & 0077U) != 0 || directoryInfo.st_dev != directoryName.st_dev || directoryInfo.st_ino != directoryName.st_ino
					|| ::fstatat(Directory.Value, aName.c_str(), &named, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(named.st_mode) || named.st_dev != held.Device || named.st_ino != held.Inode) {
					fail(failure::invalid_permissions);
				}
			}
			auto names() const -> std::vector<std::string>
			{
				handle duplicate{ ::openat(Directory.Value, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
				if (!duplicate.valid()) {
					fail(failure::io_failure);
				}
				auto* stream = ::fdopendir(duplicate.Value);
				if (!stream) {
					fail(failure::io_failure);
				}
				duplicate.Value = -1;
				struct closer
				{
					void operator()(DIR* aStream) const { ::closedir(aStream); }
				};
				std::unique_ptr<DIR, closer> owned{ stream };
				std::vector<std::string>     result;
				while (true) {
					errno            = 0;
					const auto* item = ::readdir(stream);
					if (!item) {
						if (errno != 0) {
							fail(failure::io_failure);
						}
						break;
					}
					const std::string_view name{ item->d_name };
					if (name == "." || name == ".." || name == lock_name) {
						continue;
					}
					if (result.size() == 128U) {
						fail(failure::io_failure);
					}
					result.emplace_back(name);
				}
				return result;
			}
			void remove(const handle& aFile, const std::string& aName) const
			{
				verify(aFile, aName);
				if (::unlinkat(Directory.Value, aName.c_str(), 0) != 0) {
					fail(failure::io_failure);
				}
			}

		private:
			std::filesystem::path RootPath;
			handle                Root;
			handle                Directory;
		};
		auto append(const handle& aFile, std::string_view aBytes) -> bool
		{
			while (!aBytes.empty()) {
				const auto written = ::write(aFile.Value, aBytes.data(), aBytes.size());
				if (written < 0 && errno == EINTR) {
					continue;
				}
				if (written <= 0) {
					return false;
				}
				aBytes.remove_prefix(static_cast<std::size_t>(written));
			}
			return true;
		}
#else
		namespace win = native_storage;
		using handle  = win::windows_handle;
		auto information(const handle& aFile) -> file_info
		{
			const auto identity = win::identity_of(aFile.Value, win::windows_object_kind::regular_file);
			const auto security = win::private_security::load();
			if (!identity || !security || !win::validate_private_security(aFile.Value, *security, win::windows_object_kind::regular_file)) {
				fail(failure::invalid_permissions);
			}
			FILE_STANDARD_INFO standard{ };
			FILE_BASIC_INFO    basic{ };
			if (!::GetFileInformationByHandleEx(aFile.Value, FileStandardInfo, &standard, sizeof(standard)) || !::GetFileInformationByHandleEx(aFile.Value, FileBasicInfo, &basic, sizeof(basic)) || standard.EndOfFile.QuadPart < 0) {
				fail(failure::io_failure);
			}
			return { static_cast<std::uint64_t>(standard.EndOfFile.QuadPart), basic.LastWriteTime.QuadPart / 10000000LL - 11644473600LL, *identity };
		}
		auto lock(const handle& aFile) -> bool
		{
			OVERLAPPED position{ };
			position.OffsetHigh = 0x7fffffffU;
			if (::LockFileEx(aFile.Value, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0U, 1U, 0U, &position)) {
				return true;
			}
			if (::GetLastError() != ERROR_LOCK_VIOLATION) {
				fail(failure::io_failure);
			}
			return false;
		}
		void unlock(const handle& aFile) noexcept
		{
			OVERLAPPED position{ };
			position.OffsetHigh = 0x7fffffffU;
			static_cast<void>(::UnlockFileEx(aFile.Value, 0U, 1U, 0U, &position));
		}
		class directory
		{
		public:
			explicit directory(const std::filesystem::path& aRoot)
			{
				const auto canonical = win::canonical_windows_root(aRoot);
				const auto security  = win::private_security::load();
				if (!canonical || !security) {
					fail(failure::invalid_root);
				}
				auto parents = win::validate_ancestor_chain(*canonical);
				auto root    = win::open_private_directory(*canonical, *security, false);
				if (!parents || !root || !win::reliable_local_filesystem(*root)) {
					fail(failure::invalid_permissions);
				}
				auto logs = win::ensure_private_child(*root, L"logs", *security);
				if (!logs) {
					fail(failure::invalid_permissions);
				}
				Path    = logs->Path;
				Parents = std::move(*parents);
				Parents.push_back(std::move(*root));
				Parents.push_back(std::move(*logs));
			}
			auto open(const std::string& aName, bool aCreate, bool aExclusive = false) const -> handle
			{
				const auto                       security = win::private_security::load();
				win::private_security_attributes attributes;
				if (!security || !attributes.initialize(*security, win::windows_object_kind::regular_file)) {
					fail(failure::invalid_permissions);
				}
				handle file{ ::CreateFileW((Path / aName).c_str(), GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
										   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, aCreate ? attributes.attributes() : nullptr,
										   aExclusive ? CREATE_NEW : aCreate ? OPEN_ALWAYS
																			 : OPEN_EXISTING,
										   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
				if (!file.valid()) {
					fail(failure::io_failure);
				}
				static_cast<void>(information(file));
				verify(file, aName);
				return file;
			}
			void verify(const handle& aFile, const std::string& aName) const
			{
				if (!win::path_matches_handle(aFile.Value, Path / aName)) {
					fail(failure::invalid_permissions);
				}
			}
			auto names() const -> std::vector<std::string>
			{
				std::vector<std::string> result;
				for (const auto& item : std::filesystem::directory_iterator{ Path }) {
					const auto name = item.path().filename().string();
					if (name == lock_name) {
						continue;
					}
					if (result.size() == 128U) {
						fail(failure::io_failure);
					}
					result.push_back(name);
				}
				return result;
			}
			void remove(const handle& aFile, const std::string& aName) const
			{
				verify(aFile, aName);
				// Live append handles omit DELETE access so ordinary local readers do
				// not have to opt into deletion sharing. Retention holds the inactive
				// session lock and acquires deletion authority for this exact object.
				handle removal{ ::CreateFileW((Path / aName).c_str(), DELETE | FILE_READ_ATTRIBUTES | READ_CONTROL,
											  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
											  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
				if (!removal.valid() || information(removal).Identity != information(aFile).Identity) {
					fail(failure::invalid_permissions);
				}
				verify(removal, aName);
				FILE_DISPOSITION_INFO disposition{ TRUE };
				if (!::SetFileInformationByHandle(removal.Value, FileDispositionInfo, &disposition, sizeof(disposition))) {
					fail(failure::io_failure);
				}
			}

		private:
			std::filesystem::path                    Path;
			std::vector<win::private_windows_object> Parents;
		};
		auto append(const handle& aFile, std::string_view aBytes) -> bool
		{
			LARGE_INTEGER offset{ };
			DWORD         written = 0U;
			return ::SetFilePointerEx(aFile.Value, offset, nullptr, FILE_END)
				&& ::WriteFile(aFile.Value, aBytes.data(), static_cast<DWORD>(aBytes.size()), &written, nullptr) && written == aBytes.size();
		}
#endif
		void acquire_startup_lock(const handle& aFile)
		{
			for (unsigned attempt = 0U; attempt < 100U; ++attempt) {
				if (lock(aFile)) {
					return;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
			}
			fail(failure::busy);
		}
		struct unlock_guard
		{
			const handle& File;
			~unlock_guard() { unlock(File); }
		};
		struct retained_file
		{
			std::string Name;
			handle      File;
			file_info   Info;
			bool        Active = false;
		};
		auto now_seconds() -> std::int64_t { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
	}
	class session_files::implementation
	{
	public:
		implementation(const std::filesystem::path& aRoot, std::string aSession, diagnostic_file_limits aLimits)
			: Directory{ aRoot }
			, Session{ std::move(aSession) }
			, Limits{ aLimits }
		{
			if (Limits.FileBytes == 0U || Limits.FileBytes > 5U * 1024U * 1024U || Limits.TotalBytes < Limits.FileBytes || Limits.TotalBytes > 50U * 1024U * 1024U || Limits.Files == 0U || Limits.Files > 128U || Limits.MaximumAge.count() <= 0 || Limits.MaximumAge > std::chrono::hours{ 24 * 30 }) {
				fail(failure::invalid_root);
			}
			Coordination = Directory.open(std::string{ lock_name }, true);
			if (information(Coordination).Bytes != 0U) {
				fail(failure::invalid_permissions);
			}
			acquire_startup_lock(Coordination);
			const unlock_guard release{ Coordination };
			Directory.verify(Coordination, std::string{ lock_name });
			if (!retain(0U, 1U)) {
				fail(failure::busy);
			}
			rotate();
		}
		auto write(std::string_view aBytes) -> status
		{
			if (aBytes.size() > Limits.FileBytes) {
				return status::full;
			}
			if (!lock(Coordination)) {
				return status::dropped;
			}
			const unlock_guard release{ Coordination };
			Directory.verify(Coordination, std::string{ lock_name });
			Directory.verify(Current, Name);
			const auto info = information(Current);
			if (info.Bytes > Limits.FileBytes || aBytes.size() > Limits.FileBytes - info.Bytes || now_seconds() - Created >= Limits.MaximumAge.count()) {
				Current = handle{ };
				if (!retain(aBytes.size(), 1U)) {
					return status::full;
				}
				rotate();
			} else if (!retain(aBytes.size(), 0U)) {
				return status::full;
			}
			return append(Current, aBytes) ? status::available : status::unavailable;
		}

		auto clear() -> application::diagnostic_clear_result
		{
			using result = application::diagnostic_clear_result;
			if (!lock(Coordination)) {
				return result::busy;
			}
			const unlock_guard release{ Coordination };
			Directory.verify(Coordination, std::string{ lock_name });
			auto files = inventory();
			if (std::ranges::any_of(files, [this](const auto& aFile) { return aFile.Active && aFile.Name != Name; })) {
				return result::busy;
			}
			// Admission has completed. Any subsequent I/O failure may have deleted a subset.
			try {
				for (auto& file : files) {
					Directory.remove(file.File, file.Name);
				}
				files.clear();
				Current = handle{ };
				rotate();
				return result::cleared;
			} catch (...) {
				return result::partial;
			}
		}

	private:
		auto inventory() -> std::vector<retained_file>
		{
			std::vector<retained_file> files;
			for (const auto& name : Directory.names()) {
				if (!application::diagnostic_log_file(name)) {
					fail(failure::invalid_permissions);
				}
				auto       file   = Directory.open(name, false);
				const auto info   = information(file);
				const auto active = !lock(file);
				files.push_back({ name, std::move(file), info, active });
			}
			std::ranges::sort(files, [](const auto& aLeft, const auto& aRight) { return aLeft.Info.Modified != aRight.Info.Modified ? aLeft.Info.Modified < aRight.Info.Modified : aLeft.Name < aRight.Name; });
			return files;
		}
		auto retain(std::uint64_t aIncoming, std::size_t aNewFiles) -> bool
		{
			auto          files = inventory();
			std::uint64_t total = 0U;
			for (const auto& file : files) {
				if (file.Info.Bytes > std::numeric_limits<std::uint64_t>::max() - total) {
					fail(failure::io_failure);
				}
				total += file.Info.Bytes;
			}
			auto       count = files.size();
			const auto now   = now_seconds();
			for (auto& file : files) {
				const auto overBudget = total > Limits.TotalBytes - aIncoming || count + aNewFiles > Limits.Files;
				const auto expired    = file.Info.Modified <= now && static_cast<std::uint64_t>(now) - static_cast<std::uint64_t>(file.Info.Modified) >= static_cast<std::uint64_t>(Limits.MaximumAge.count());
				if (!file.Active && (overBudget || expired || file.Info.Bytes > Limits.FileBytes)) {
					Directory.remove(file.File, file.Name);
					file.File = handle{ };
					total -= file.Info.Bytes;
					--count;
				}
			}
			return total <= Limits.TotalBytes - aIncoming && count + aNewFiles <= Limits.Files;
		}
		void rotate()
		{
			if (Sequence == std::numeric_limits<std::uint32_t>::max()) {
				fail(failure::io_failure);
			}
			std::array<char, 8>        suffix{ };
			const auto                 sequence = ++Sequence;
			constexpr std::string_view hex      = "0123456789abcdef";
			for (unsigned index = 0U; index < 8U; ++index) {
				suffix[7U - index] = hex[(sequence >> (index * 4U)) & 15U];
			}
			Name    = "session-" + Session + "-" + std::string{ suffix.data(), suffix.size() } + ".jsonl";
			Current = Directory.open(Name, true, true);
			if (!lock(Current)) {
				fail(failure::busy);
			}
			Created = now_seconds();
		}
		directory              Directory;
		std::string            Session;
		diagnostic_file_limits Limits;
		handle                 Coordination;
		handle                 Current;
		std::string            Name;
		std::uint32_t          Sequence = 0U;
		std::int64_t           Created  = 0;
	};
	session_files::session_files(const std::filesystem::path& aRoot, std::string aSession, diagnostic_file_limits aLimits)
	{
		try {
			Implementation = std::make_unique<implementation>(aRoot, std::move(aSession), aLimits);
		} catch (const storage_error& aFailure) {
			OpenFailure = aFailure.Reason;
		} catch (...) {
			OpenFailure = diagnostic_file_failure::io_failure;
		}
	}
	session_files::~session_files() = default;
	auto session_files::clear() noexcept -> application::diagnostic_clear_result
	{
		try {
			return Implementation ? Implementation->clear() : application::diagnostic_clear_result::unavailable;
		} catch (...) {
			return application::diagnostic_clear_result::unavailable;
		}
	}
	auto session_files::failure() const noexcept -> diagnostic_file_failure { return OpenFailure; }
	auto session_files::write(std::string_view aBytes) noexcept -> application::diagnostic_log_status
	{
		try {
			return Implementation ? Implementation->write(aBytes) : status::unavailable;
		} catch (...) {
			return status::unavailable;
		}
	}
}
