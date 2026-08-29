#ifndef BPR_TESTS_SQLITE_CONTRACT_PLATFORM_TEST_SUPPORT_HPP
#define BPR_TESTS_SQLITE_CONTRACT_PLATFORM_TEST_SUPPORT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <aclapi.h>
	#include <windows.h>
	#include <winioctl.h>
#else
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::tests::sqlite_contract {
	[[nodiscard]] inline auto process_id() noexcept -> std::uint64_t
	{
#if defined(_WIN32)
		return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
		return static_cast<std::uint64_t>(::getpid());
#endif
	}

	[[nodiscard]] inline auto sqlite_path(const std::filesystem::path& aPath)
		-> std::string
	{
#if defined(_WIN32)
		const auto encoded = aPath.u8string();
		return {
			reinterpret_cast<const char*>(encoded.data()),
			encoded.size(),
		};
#else
		return aPath.string();
#endif
	}

	[[nodiscard]] inline constexpr auto sqlite_vfs() noexcept -> const char*
	{
#if defined(_WIN32)
		return "win32-longpath";
#else
		return nullptr;
#endif
	}

	[[nodiscard]] inline auto with_ascii_suffix(
		const std::filesystem::path& aPath,
		const std::string_view       aSuffix) -> std::filesystem::path
	{
#if defined(_WIN32)
		auto native = aPath.native();
		for (const auto character : aSuffix) {
			native.push_back(static_cast<wchar_t>(
				static_cast<unsigned char>(character)));
		}
		return std::filesystem::path{ native };
#else
		return std::filesystem::path{ aPath.string() + std::string{ aSuffix } };
#endif
	}

#if defined(_WIN32)
	class unique_handle final
	{
	public:
		explicit unique_handle(HANDLE value = INVALID_HANDLE_VALUE) noexcept
			: Value{ value }
		{
		}

		unique_handle(const unique_handle&)                    = delete;
		auto operator=(const unique_handle&) -> unique_handle& = delete;

		unique_handle(unique_handle&& other) noexcept
			: Value{ other.Value }
		{
			other.Value = INVALID_HANDLE_VALUE;
		}

		auto operator=(unique_handle&& other) noexcept -> unique_handle&
		{
			if (this != &other) {
				reset();
				Value       = other.Value;
				other.Value = INVALID_HANDLE_VALUE;
			}
			return *this;
		}

		~unique_handle()
		{
			reset();
		}

		[[nodiscard]] auto get() const noexcept -> HANDLE
		{
			return Value;
		}

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return Value != nullptr && Value != INVALID_HANDLE_VALUE;
		}

	private:
		void reset() noexcept
		{
			if (*this) {
				(void)::CloseHandle(Value);
			}
			Value = INVALID_HANDLE_VALUE;
		}

		HANDLE Value = INVALID_HANDLE_VALUE;
	};

	[[nodiscard]] inline auto current_user_sid_storage()
		-> std::vector<std::byte>
	{
		HANDLE raw_token = nullptr;
		if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token) == 0) {
			return { };
		}
		unique_handle token{ raw_token };
		DWORD         required = 0;
		(void)::GetTokenInformation(token.get(), TokenUser, nullptr, 0, &required);
		if (required == 0 || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return { };
		}
		auto storage = std::vector<std::byte>(required);
		if (::GetTokenInformation(
				token.get(),
				TokenUser,
				storage.data(),
				required,
				&required)
			== 0) {
			return { };
		}
		return storage;
	}

	// An owner-only descriptor is unusable from inside an AppContainer: a lowbox
	// token's access check additionally requires the object to grant the
	// container's own package SID. Returns the package SID's bytes when this
	// process is contained, and an empty vector when it is not, so that callers
	// can distinguish "not contained" from "could not tell".
	[[nodiscard]] inline auto current_appcontainer_sid_storage()
		-> std::vector<std::byte>
	{
		HANDLE raw_token = nullptr;
		if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token) == 0) {
			return { };
		}
		unique_handle token{ raw_token };
		DWORD         contained = 0;
		DWORD         returned  = 0;
		if (::GetTokenInformation(
				token.get(),
				TokenIsAppContainer,
				&contained,
				sizeof(contained),
				&returned)
				== 0
			|| contained == 0) {
			return { };
		}

		DWORD required = 0;
		(void)::GetTokenInformation(
			token.get(), TokenAppContainerSid, nullptr, 0, &required);
		if (required == 0 || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			return { };
		}
		auto information = std::vector<std::byte>(required);
		if (::GetTokenInformation(
				token.get(),
				TokenAppContainerSid,
				information.data(),
				required,
				&required)
			== 0) {
			return { };
		}
		const auto* container = reinterpret_cast<const TOKEN_APPCONTAINER_INFORMATION*>(
			information.data());
		if (container->TokenAppContainer == nullptr
			|| ::IsValidSid(container->TokenAppContainer) == 0) {
			return { };
		}

		// Copy the SID out rather than handing back a pointer into the token
		// buffer, so the returned bytes stand on their own.
		const auto length  = ::GetLengthSid(container->TokenAppContainer);
		auto       storage = std::vector<std::byte>(length);
		if (::CopySid(
				length,
				static_cast<PSID>(storage.data()),
				container->TokenAppContainer)
			== 0) {
			return { };
		}
		return storage;
	}

	[[nodiscard]] inline auto set_test_dacl(
		const std::filesystem::path& path,
		const bool                   directory,
		const bool                   permissive) -> bool
	{
		auto user_storage = current_user_sid_storage();
		if (user_storage.empty()) {
			return false;
		}
		const auto* token_user = reinterpret_cast<const TOKEN_USER*>(user_storage.data());
		if (::IsValidSid(token_user->User.Sid) == 0) {
			return false;
		}

		// The container is not a second user. Granting its package SID keeps the
		// object reachable by the only identity that is supposed to own it,
		// which an owner-only descriptor otherwise locks out of its own
		// directory.
		auto container_storage = current_appcontainer_sid_storage();

		auto entries                    = std::array<EXPLICIT_ACCESSW, 3U>{ };
		entries[0].grfAccessPermissions = FILE_ALL_ACCESS;
		entries[0].grfAccessMode        = SET_ACCESS;
		entries[0].grfInheritance       = directory
			? SUB_CONTAINERS_AND_OBJECTS_INHERIT
			: NO_INHERITANCE;
		entries[0].Trustee.TrusteeForm  = TRUSTEE_IS_SID;
		entries[0].Trustee.TrusteeType  = TRUSTEE_IS_USER;
		entries[0].Trustee.ptstrName    = static_cast<LPWSTR>(token_user->User.Sid);

		auto entry_count = ULONG{ 1 };
		if (!container_storage.empty()) {
			entries[entry_count].grfAccessPermissions = FILE_ALL_ACCESS;
			entries[entry_count].grfAccessMode        = GRANT_ACCESS;
			entries[entry_count].grfInheritance       = directory
				? SUB_CONTAINERS_AND_OBJECTS_INHERIT
				: NO_INHERITANCE;
			entries[entry_count].Trustee.TrusteeForm  = TRUSTEE_IS_SID;
			entries[entry_count].Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
			entries[entry_count].Trustee.ptstrName    = reinterpret_cast<LPWSTR>(
				container_storage.data());
			++entry_count;
		}

		auto  world_storage = std::array<std::byte, SECURITY_MAX_SID_SIZE>{ };
		DWORD world_size    = static_cast<DWORD>(world_storage.size());
		if (permissive) {
			if (::CreateWellKnownSid(
					WinWorldSid,
					nullptr,
					world_storage.data(),
					&world_size)
				== 0) {
				return false;
			}
			entries[entry_count].grfAccessPermissions = FILE_GENERIC_READ;
			entries[entry_count].grfAccessMode        = GRANT_ACCESS;
			entries[entry_count].grfInheritance       = directory
				? SUB_CONTAINERS_AND_OBJECTS_INHERIT
				: NO_INHERITANCE;
			entries[entry_count].Trustee.TrusteeForm  = TRUSTEE_IS_SID;
			entries[entry_count].Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
			entries[entry_count].Trustee.ptstrName    = reinterpret_cast<LPWSTR>(
				world_storage.data());
			++entry_count;
		}

		PACL acl = nullptr;
		if (::SetEntriesInAclW(entry_count, entries.data(), nullptr, &acl)
			!= ERROR_SUCCESS) {
			return false;
		}
		auto       native = path.native();
		const auto result = ::SetNamedSecurityInfoW(
			native.data(),
			SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
			nullptr,
			nullptr,
			acl,
			nullptr);
		(void)::LocalFree(acl);
		if (result != ERROR_SUCCESS) {
			return false;
		}
		return ::SetNamedSecurityInfoW(
				   native.data(),
				   SE_FILE_OBJECT,
				   OWNER_SECURITY_INFORMATION,
				   token_user->User.Sid,
				   nullptr,
				   nullptr,
				   nullptr)
			== ERROR_SUCCESS;
	}

	[[nodiscard]] inline auto make_owner_private(
		const std::filesystem::path& path,
		const bool                   directory) -> bool
	{
		return set_test_dacl(path, directory, false);
	}

	[[nodiscard]] inline auto make_permissive(
		const std::filesystem::path& path,
		const bool                   directory) -> bool
	{
		return set_test_dacl(path, directory, true);
	}

	[[nodiscard]] inline auto is_owner_private(
		const std::filesystem::path& path,
		const bool                   directory) -> bool
	{
		auto user_storage = current_user_sid_storage();
		if (user_storage.empty()) {
			return false;
		}
		auto container_storage = current_appcontainer_sid_storage();
		// Inside a container the owner-private descriptor carries exactly one
		// extra entry, for the package SID that the restricted access check
		// requires. Anything beyond that pair is not owner-private.
		const auto expected_ace_count
			= static_cast<ULONG>(container_storage.empty() ? 1 : 2);
		const auto*          token_user = reinterpret_cast<const TOKEN_USER*>(user_storage.data());
		auto                 native     = path.native();
		PSID                 owner      = nullptr;
		PACL                 dacl       = nullptr;
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		const auto           result     = ::GetNamedSecurityInfoW(
			native.data(),
			SE_FILE_OBJECT,
			OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
			&owner,
			nullptr,
			&dacl,
			nullptr,
			&descriptor);
		if (result != ERROR_SUCCESS || descriptor == nullptr || owner == nullptr
			|| dacl == nullptr) {
			if (descriptor != nullptr) {
				(void)::LocalFree(descriptor);
			}
			return false;
		}

		SECURITY_DESCRIPTOR_CONTROL control  = 0;
		DWORD                       revision = 0;
		ACL_SIZE_INFORMATION        information{ };
		const auto                  descriptor_valid = ::EqualSid(owner, token_user->User.Sid) != 0
			&& ::GetSecurityDescriptorControl(
				   descriptor, &control, &revision)
				!= 0
			&& (control & SE_DACL_PROTECTED) != 0
			&& ::GetAclInformation(
				   dacl,
				   &information,
				   sizeof(information),
				   AclSizeInformation)
				!= 0
			&& information.AceCount == expected_ace_count;
		if (!descriptor_valid) {
			(void)::LocalFree(descriptor);
			return false;
		}

		const auto expected_flags = static_cast<BYTE>(
			directory ? OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE : 0);
		// Require each expected trustee exactly once, without depending on the
		// order SetEntriesInAclW happened to emit them in.
		auto user_grants      = 0;
		auto container_grants = 0;
		auto every_ace_valid  = true;
		for (auto index = ULONG{ 0 }; index < expected_ace_count; ++index) {
			void* raw_ace = nullptr;
			if (::GetAce(dacl, index, &raw_ace) == 0 || raw_ace == nullptr) {
				every_ace_valid = false;
				break;
			}
			auto* header  = static_cast<ACE_HEADER*>(raw_ace);
			auto* allowed = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
			if (header->AceType != ACCESS_ALLOWED_ACE_TYPE
				|| header->AceFlags != expected_flags
				|| allowed->Mask != FILE_ALL_ACCESS) {
				every_ace_valid = false;
				break;
			}
			auto* granted = static_cast<PSID>(&allowed->SidStart);
			if (::EqualSid(granted, token_user->User.Sid) != 0) {
				++user_grants;
			} else if (
				!container_storage.empty()
				&& ::EqualSid(
					   granted,
					   static_cast<PSID>(container_storage.data()))
					!= 0) {
				++container_grants;
			} else {
				every_ace_valid = false;
				break;
			}
		}
		(void)::LocalFree(descriptor);
		return every_ace_valid && user_grants == 1
			&& container_grants == (container_storage.empty() ? 0 : 1);
	}

	struct file_identity
	{
		std::uint64_t              Volume = 0;
		std::array<std::byte, 16U> File{ };

		auto operator<=>(const file_identity&) const = default;
	};

	[[nodiscard]] inline auto identity_of(const std::filesystem::path& path)
		-> std::optional<file_identity>
	{
		auto          native = path.native();
		unique_handle handle{ ::CreateFileW(
			native.c_str(),
			FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
			nullptr) };
		if (!handle) {
			return std::nullopt;
		}
		FILE_ID_INFO information{ };
		if (::GetFileInformationByHandleEx(
				handle.get(), FileIdInfo, &information, sizeof(information))
			== 0) {
			return std::nullopt;
		}
		file_identity output{ .Volume = information.VolumeSerialNumber };
		for (std::size_t index = 0; index < output.File.size(); ++index) {
			output.File[index] = static_cast<std::byte>(information.FileId.Identifier[index]);
		}
		return output;
	}

	enum class symbolic_link_result : std::uint8_t {
		created,
		privilege_unavailable,
		failed,
	};

	[[nodiscard]] inline auto create_file_symbolic_link(
		const std::filesystem::path& link,
		const std::filesystem::path& target) -> symbolic_link_result
	{
		constexpr DWORD AllowUnprivilegedCreate = 0x2U;
		if (::CreateSymbolicLinkW(
				link.c_str(), target.c_str(), AllowUnprivilegedCreate)
			!= 0) {
			return symbolic_link_result::created;
		}
		auto error = ::GetLastError();
		if (error == ERROR_INVALID_PARAMETER) {
			if (::CreateSymbolicLinkW(link.c_str(), target.c_str(), 0) != 0) {
				return symbolic_link_result::created;
			}
			error = ::GetLastError();
		}
		if (error == ERROR_PRIVILEGE_NOT_HELD) {
			return symbolic_link_result::privilege_unavailable;
		}
		return symbolic_link_result::failed;
	}

	[[nodiscard]] inline auto create_directory_junction(
		const std::filesystem::path& link,
		const std::filesystem::path& target) -> bool
	{
		if (::CreateDirectoryW(link.c_str(), nullptr) == 0) {
			return false;
		}
		unique_handle handle{ ::CreateFileW(
			link.c_str(),
			GENERIC_WRITE,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
			nullptr) };
		if (!handle) {
			(void)::RemoveDirectoryW(link.c_str());
			return false;
		}

		const auto substitute       = std::wstring{ L"\\??\\" } + target.native();
		const auto print            = target.native();
		const auto substitute_bytes = static_cast<USHORT>(
			substitute.size() * sizeof(wchar_t));
		const auto print_bytes = static_cast<USHORT>(print.size() * sizeof(wchar_t));
		const auto path_bytes  = static_cast<std::size_t>(substitute_bytes)
			+ sizeof(wchar_t) + static_cast<std::size_t>(print_bytes)
			+ sizeof(wchar_t);
		struct mount_point_buffer
		{
			DWORD   ReparseTag;
			WORD    ReparseDataLength;
			WORD    Reserved;
			WORD    SubstituteNameOffset;
			WORD    SubstituteNameLength;
			WORD    PrintNameOffset;
			WORD    PrintNameLength;
			wchar_t PathBuffer[1];
		};
		const auto total_bytes = offsetof(mount_point_buffer, PathBuffer)
			+ path_bytes;
		auto  storage              = std::vector<std::byte>(total_bytes);
		auto* data                 = reinterpret_cast<mount_point_buffer*>(storage.data());
		data->ReparseTag           = IO_REPARSE_TAG_MOUNT_POINT;
		data->ReparseDataLength    = static_cast<WORD>(total_bytes - 8U);
		data->Reserved             = 0;
		data->SubstituteNameOffset = 0;
		data->SubstituteNameLength = substitute_bytes;
		data->PrintNameOffset      = static_cast<WORD>(
			substitute_bytes + sizeof(wchar_t));
		data->PrintNameLength = print_bytes;
		std::memcpy(data->PathBuffer, substitute.data(), substitute_bytes);
		std::memcpy(
			reinterpret_cast<std::byte*>(data->PathBuffer)
				+ data->PrintNameOffset,
			print.data(),
			print_bytes);
		DWORD      returned = 0;
		const auto set      = ::DeviceIoControl(
								  handle.get(),
								  FSCTL_SET_REPARSE_POINT,
								  data,
								  static_cast<DWORD>(total_bytes),
								  nullptr,
								  0,
								  &returned,
								  nullptr)
			!= 0;
		if (!set) {
			handle = unique_handle{ };
			(void)::RemoveDirectoryW(link.c_str());
		}
		return set;
	}
#else
	[[nodiscard]] inline auto make_owner_private(
		const std::filesystem::path& aPath,
		const bool                   aDirectory) -> bool
	{
		std::error_code error;
		std::filesystem::permissions(
			aPath,
			aDirectory ? std::filesystem::perms::owner_all
					   : std::filesystem::perms::owner_read
					| std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace,
			error);
		return !error;
	}

	[[nodiscard]] inline auto make_permissive(
		const std::filesystem::path& aPath,
		const bool                   aDirectory) -> bool
	{
		std::error_code error;
		std::filesystem::permissions(
			aPath,
			(aDirectory ? std::filesystem::perms::owner_all
						: std::filesystem::perms::owner_read
					 | std::filesystem::perms::owner_write)
				| std::filesystem::perms::group_read,
			std::filesystem::perm_options::replace,
			error);
		return !error;
	}

	[[nodiscard]] inline auto is_owner_private(
		const std::filesystem::path& aPath,
		const bool                   aDirectory) -> bool
	{
		struct stat status{ };
		return ::lstat(aPath.c_str(), &status) == 0
			&& (status.st_mode & static_cast<mode_t>(0777)) == (aDirectory ? 0700 : 0600);
	}

	struct file_identity
	{
		std::uint64_t Device = 0;
		std::uint64_t Inode  = 0;

		auto operator<=>(const file_identity&) const = default;
	};

	[[nodiscard]] inline auto identity_of(const std::filesystem::path& aPath)
		-> std::optional<file_identity>
	{
		struct stat status{ };
		if (::lstat(aPath.c_str(), &status) != 0) {
			return std::nullopt;
		}
		return file_identity{
			.Device = static_cast<std::uint64_t>(status.st_dev),
			.Inode  = static_cast<std::uint64_t>(status.st_ino),
		};
	}
#endif
} // namespace bpr::tests::sqlite_contract

#endif // BPR_TESTS_SQLITE_CONTRACT_PLATFORM_TEST_SUPPORT_HPP
