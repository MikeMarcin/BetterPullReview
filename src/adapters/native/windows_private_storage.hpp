#ifndef BPR_ADAPTERS_NATIVE_WINDOWS_PRIVATE_STORAGE_HPP
#define BPR_ADAPTERS_NATIVE_WINDOWS_PRIVATE_STORAGE_HPP

#if defined(_WIN32)
	#include "bpr/platform/windows_private_object.hpp"
	#include "storage_failure.hpp"
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <cstdint>
	#include <expected>
	#include <filesystem>
	#include <limits>
	#include <string>
	#include <string_view>
	#include <utility>
	#include <vector>
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
	#include <aclapi.h>

// Shared native admission primitives, extracted unchanged from the SQLite owner.
// This effect-only utility retains the established typed storage failure vocabulary;
// it contains no catalog, maintenance, logging or credential policy.
namespace bpr::adapters::native_storage {
	inline constexpr std::size_t MaximumWindowsPathCharacters  = 32'767U;
	inline constexpr std::size_t ReservedCatalogPathCharacters = 128U;
	// [[BPR-0825]]. The owner-private descriptor, and its check, are the
	// platform target's one definition; the Git adapter admits a root this
	// adapter created only through the same one.
	using windows_object_kind = ::bpr::platform::windows_private_object::object_kind;

	class windows_handle final
	{
	public:
		windows_handle() = default;
		explicit windows_handle(const HANDLE value) noexcept
			: Value{ value }
		{ }
		windows_handle(const windows_handle&)                    = delete;
		auto operator=(const windows_handle&) -> windows_handle& = delete;
		windows_handle(windows_handle&& other) noexcept
			: Value{ std::exchange(other.Value, INVALID_HANDLE_VALUE) }
		{
		}
		auto operator=(windows_handle&& other) noexcept -> windows_handle&
		{
			if (this != &other) {
				reset();
				Value = std::exchange(other.Value, INVALID_HANDLE_VALUE);
			}
			return *this;
		}
		~windows_handle()
		{
			reset();
		}

		[[nodiscard]] auto valid() const noexcept -> bool
		{
			return Value != nullptr && Value != INVALID_HANDLE_VALUE;
		}

		void reset() noexcept
		{
			if (valid()) {
				(void)::CloseHandle(Value);
			}
			Value = INVALID_HANDLE_VALUE;
		}

		HANDLE Value = INVALID_HANDLE_VALUE;
	};

	class local_memory final
	{
	public:
		local_memory() = default;
		explicit local_memory(void* value) noexcept
			: Value{ value }
		{ }
		local_memory(const local_memory&)                    = delete;
		auto operator=(const local_memory&) -> local_memory& = delete;
		~local_memory()
		{
			if (Value != nullptr) {
				(void)::LocalFree(Value);
			}
		}

		void* Value = nullptr;
	};

	[[nodiscard]] inline auto windows_error_failure(
		const DWORD   error,
		const failure fallback = failure::unavailable) noexcept -> failure
	{
		switch (error) {
		case ERROR_ACCESS_DENIED:
		case ERROR_PRIVILEGE_NOT_HELD:
			return failure::invalid_permissions;
		case ERROR_BAD_PATHNAME:
		case ERROR_DIRECTORY:
		case ERROR_FILE_NOT_FOUND:
		case ERROR_INVALID_DRIVE:
		case ERROR_INVALID_NAME:
		case ERROR_PATH_NOT_FOUND:
			return failure::invalid_path;
		case ERROR_INVALID_FUNCTION:
		case ERROR_NOT_SUPPORTED:
			return failure::platform_unsupported;
		default:
			return fallback;
		}
	}

	[[nodiscard]] inline auto same_windows_path(
		const std::wstring_view left,
		const std::wstring_view right) noexcept -> bool
	{
		if (left.size() != right.size()
			|| left.size()
				> static_cast<std::size_t>(std::numeric_limits<int>::max())) {
			return false;
		}
		return ::CompareStringOrdinal(
				   left.data(),
				   static_cast<int>(left.size()),
				   right.data(),
				   static_cast<int>(right.size()),
				   FALSE)
			== CSTR_EQUAL;
	}

	[[nodiscard]] inline auto final_path_of(const HANDLE handle)
		-> std::expected<std::wstring, failure>
	{
		constexpr DWORD Flags    = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
		const auto      required = ::GetFinalPathNameByHandleW(handle, nullptr, 0U, Flags);
		if (required == 0U
			|| required >= static_cast<DWORD>(MaximumWindowsPathCharacters)) {
			return std::unexpected(windows_error_failure(::GetLastError()));
		}
		auto buffer = std::vector<wchar_t>(
			static_cast<std::size_t>(required) + 1U, L'\0');
		const auto written = ::GetFinalPathNameByHandleW(
			handle,
			buffer.data(),
			static_cast<DWORD>(buffer.size()),
			Flags);
		if (written == 0U || written >= buffer.size()) {
			return std::unexpected(windows_error_failure(::GetLastError()));
		}
		return std::wstring{ buffer.data(), static_cast<std::size_t>(written) };
	}

	[[nodiscard]] inline auto path_matches_handle(
		const HANDLE                 handle,
		const std::filesystem::path& expected) -> std::expected<void, failure>
	{
		const auto actual = final_path_of(handle);
		if (!actual || !same_windows_path(*actual, expected.native())) {
			return std::unexpected(
				actual ? failure::invalid_path : actual.error());
		}
		return { };
	}

	class private_security final
	{
	public:
		private_security()                                               = default;
		private_security(const private_security&)                        = default;
		auto operator=(const private_security&) -> private_security&     = default;
		private_security(private_security&&) noexcept                    = default;
		auto operator=(private_security&&) noexcept -> private_security& = default;

		[[nodiscard]] static auto load()
			-> std::expected<private_security, failure>
		{
			windows_handle token;
			if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token.Value)
				== 0) {
				return std::unexpected(
					windows_error_failure(::GetLastError()));
			}
			DWORD required = 0U;
			(void)::GetTokenInformation(
				token.Value, TokenUser, nullptr, 0U, &required);
			if (required == 0U || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				return std::unexpected(failure::unavailable);
			}
			auto information = std::vector<std::byte>(required);
			if (::GetTokenInformation(
					token.Value,
					TokenUser,
					information.data(),
					required,
					&required)
				== 0) {
				return std::unexpected(
					windows_error_failure(::GetLastError()));
			}
			const auto* user = reinterpret_cast<const TOKEN_USER*>(
				information.data());
			if (user->User.Sid == nullptr || ::IsValidSid(user->User.Sid) == 0) {
				return std::unexpected(failure::invalid_permissions);
			}
			const auto sid_bytes = ::GetLengthSid(user->User.Sid);
			if (sid_bytes == 0U) {
				return std::unexpected(failure::invalid_permissions);
			}
			private_security result;
			result.Sid = std::vector<std::byte>(sid_bytes);
			if (::CopySid(sid_bytes, result.Sid.data(), user->User.Sid) == 0) {
				return std::unexpected(failure::unavailable);
			}
			// A lowbox token's access check additionally requires the object to
			// grant the container's own package SID. The container is recorded
			// beside the user here so that every owner-private object this
			// adapter writes names the identity the process actually runs as.
			DWORD contained = 0U;
			DWORD returned  = 0U;
			if (::GetTokenInformation(
					token.Value,
					TokenIsAppContainer,
					&contained,
					sizeof(contained),
					&returned)
				== 0) {
				return std::unexpected(
					windows_error_failure(::GetLastError()));
			}
			if (contained == 0U) {
				return result;
			}
			DWORD container_required = 0U;
			(void)::GetTokenInformation(
				token.Value,
				TokenAppContainerSid,
				nullptr,
				0U,
				&container_required);
			if (container_required == 0U
				|| ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				return std::unexpected(failure::unavailable);
			}
			auto container_information = std::vector<std::byte>(
				container_required);
			if (::GetTokenInformation(
					token.Value,
					TokenAppContainerSid,
					container_information.data(),
					container_required,
					&container_required)
				== 0) {
				return std::unexpected(
					windows_error_failure(::GetLastError()));
			}
			const auto* container
				= reinterpret_cast<const TOKEN_APPCONTAINER_INFORMATION*>(
					container_information.data());
			if (container->TokenAppContainer == nullptr
				|| ::IsValidSid(container->TokenAppContainer) == 0) {
				return std::unexpected(failure::invalid_permissions);
			}
			const auto container_bytes = ::GetLengthSid(
				container->TokenAppContainer);
			if (container_bytes == 0U) {
				return std::unexpected(failure::invalid_permissions);
			}
			result.ContainerSid = std::vector<std::byte>(container_bytes);
			if (::CopySid(
					container_bytes,
					result.ContainerSid.data(),
					container->TokenAppContainer)
				== 0) {
				return std::unexpected(failure::unavailable);
			}
			return result;
		}

		[[nodiscard]] auto sid() const noexcept -> PSID
		{
			return const_cast<std::byte*>(Sid.data());
		}

		// Null unless this process runs in an AppContainer.
		[[nodiscard]] auto container_sid() const noexcept -> PSID
		{
			return ContainerSid.empty()
				? nullptr
				: static_cast<PSID>(const_cast<std::byte*>(ContainerSid.data()));
		}

		[[nodiscard]] auto contained() const noexcept -> bool
		{
			return !ContainerSid.empty();
		}

	private:
		std::vector<std::byte> Sid;
		std::vector<std::byte> ContainerSid;
	};

	// [[BPR-0825]]. The descriptor itself is the platform target's; this keeps
	// the storage adapters' call shape and ties it to their principals.
	class private_security_attributes final
	{
	public:
		[[nodiscard]] auto initialize(
			const private_security&   security,
			const windows_object_kind kind) -> bool
		{
			return Descriptor.initialize(security.sid(), security.container_sid(), kind);
		}

		/// \brief Refused: the descriptor's owner is a pointer into the
		/// security, which a temporary would leave dangling once this returns
		/// ([[BPR-1030]]).
		auto initialize(const private_security&&, windows_object_kind) -> bool = delete;

		[[nodiscard]] auto attributes() noexcept -> SECURITY_ATTRIBUTES*
		{
			return Descriptor.attributes();
		}

		[[nodiscard]] auto acl() noexcept -> PACL
		{
			return Descriptor.acl();
		}

	private:
		::bpr::platform::windows_private_object::descriptor Descriptor;
	};

	[[nodiscard]] inline auto validate_private_security(
		const HANDLE              handle,
		const private_security&   expected,
		const windows_object_kind kind,
		const bool                allow_inherited = false) -> std::expected<void, failure>
	{
		namespace object   = ::bpr::platform::windows_private_object;
		const auto checked = object::validate(
			handle, expected.sid(), expected.container_sid(), kind, allow_inherited);
		if (checked.Verdict == object::verdict::unreadable) {
			return std::unexpected(windows_error_failure(checked.Error));
		}
		if (checked.Verdict != object::verdict::owner_private) {
			return std::unexpected(failure::invalid_permissions);
		}
		return { };
	}

	[[nodiscard]] inline auto protect_inherited_private_file(
		const HANDLE            handle,
		const private_security& security) -> std::expected<void, failure>
	{
		if (auto private_result = validate_private_security(
				handle, security, windows_object_kind::regular_file, true);
			!private_result) {
			return private_result;
		}
		private_security_attributes attributes;
		if (!attributes.initialize(security, windows_object_kind::regular_file)) {
			return std::unexpected(failure::unavailable);
		}
		const auto status = ::SetSecurityInfo(
			handle,
			SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
			nullptr,
			nullptr,
			attributes.acl(),
			nullptr);
		if (status != ERROR_SUCCESS) {
			return std::unexpected(windows_error_failure(status));
		}
		return validate_private_security(
			handle, security, windows_object_kind::regular_file);
	}

	struct file_identity final
	{
		std::uint64_t                 Volume = 0U;
		std::array<std::uint8_t, 16U> Object{ };
		windows_object_kind           Kind = windows_object_kind::regular_file;

		auto operator==(const file_identity&) const -> bool = default;
	};

	[[nodiscard]] inline auto identity_of(
		const HANDLE              handle,
		const windows_object_kind expected_kind)
		-> std::expected<file_identity, failure>
	{
		FILE_ATTRIBUTE_TAG_INFO attributes{ };
		FILE_STANDARD_INFO      standard{ };
		FILE_ID_INFO            identity{ };
		if (::GetFileInformationByHandleEx(
				handle,
				FileAttributeTagInfo,
				&attributes,
				sizeof(attributes))
				== 0
			|| ::GetFileInformationByHandleEx(
				   handle, FileStandardInfo, &standard, sizeof(standard))
				== 0
			|| ::GetFileInformationByHandleEx(
				   handle, FileIdInfo, &identity, sizeof(identity))
				== 0) {
			return std::unexpected(
				windows_error_failure(::GetLastError(), failure::invalid_path));
		}
		const auto is_directory = (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
		if ((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
			|| standard.DeletePending != FALSE
			|| is_directory
				!= (expected_kind == windows_object_kind::directory)
			|| (expected_kind == windows_object_kind::regular_file
				&& standard.NumberOfLinks != 1U)) {
			return std::unexpected(failure::invalid_path);
		}
		file_identity result{
			.Volume = identity.VolumeSerialNumber,
			.Kind   = expected_kind,
		};
		std::ranges::copy(identity.FileId.Identifier, result.Object.begin());
		if (result.Volume == 0U
			|| std::ranges::all_of(result.Object, [](const auto value) {
				   return value == 0U;
			   })) {
			return std::unexpected(failure::invalid_path);
		}
		return result;
	}

	struct private_windows_object final
	{
		windows_handle        Handle;
		std::filesystem::path Path;
		file_identity         Identity;
	};

	[[nodiscard]] inline auto open_existing_object(
		const std::filesystem::path& path,
		const windows_object_kind    kind,
		const DWORD                  additional_access    = 0U,
		const bool                   allow_delete_sharing = true)
		-> std::expected<windows_handle, failure>
	{
		(void)kind;
		const auto     flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
		windows_handle handle{ ::CreateFileW(
			path.c_str(),
			FILE_READ_ATTRIBUTES | READ_CONTROL | additional_access,
			FILE_SHARE_READ | FILE_SHARE_WRITE
				| (allow_delete_sharing ? FILE_SHARE_DELETE : 0U),
			nullptr,
			OPEN_EXISTING,
			flags,
			nullptr) };
		if (!handle.valid()) {
			return std::unexpected(windows_error_failure(::GetLastError()));
		}
		return handle;
	}

	[[nodiscard]] inline auto open_private_object(
		const std::filesystem::path& path,
		const windows_object_kind    kind,
		const private_security&      security,
		const DWORD                  additional_access    = 0U,
		const bool                   protect_inherited    = false,
		const bool                   allow_delete_sharing = true)
		-> std::expected<private_windows_object, failure>
	{
		auto opened = open_existing_object(
			path, kind, additional_access, allow_delete_sharing);
		if (!opened) {
			return std::unexpected(opened.error());
		}
		auto identity = identity_of(opened->Value, kind);
		if (!identity) {
			return std::unexpected(identity.error());
		}
		if (auto matches = path_matches_handle(opened->Value, path); !matches) {
			return std::unexpected(matches.error());
		}
		const auto permissions = protect_inherited
			? protect_inherited_private_file(opened->Value, security)
			: validate_private_security(opened->Value, security, kind);
		if (!permissions) {
			return std::unexpected(permissions.error());
		}
		return private_windows_object{
			.Handle   = std::move(*opened),
			.Path     = path,
			.Identity = *identity,
		};
	}

	[[nodiscard]] inline auto component_is_reserved(std::wstring_view component) noexcept
		-> bool
	{
		const auto dot = component.find(L'.');
		component      = component.substr(0U, dot);
		auto upper     = std::array<wchar_t, 4U>{ };
		if (component.size() > upper.size()) {
			return false;
		}
		for (std::size_t index = 0U; index < component.size(); ++index) {
			const auto value = component[index];
			upper[index]     = value >= L'a' && value <= L'z'
				? static_cast<wchar_t>(value - L'a' + L'A')
				: value;
		}
		const auto name = std::wstring_view{ upper.data(), component.size() };
		if (name == L"CON" || name == L"PRN" || name == L"AUX"
			|| name == L"NUL") {
			return true;
		}
		return name.size() == 4U
			&& (name.starts_with(L"COM") || name.starts_with(L"LPT"))
			&& name[3U] >= L'1' && name[3U] <= L'9';
	}

	[[nodiscard]] inline auto valid_windows_components(
		const std::wstring_view path,
		const std::size_t       maximum_component = 255U) noexcept -> bool
	{
		if (path.size() < 3U || path[1U] != L':' || path[2U] != L'\\'
			|| !((path[0U] >= L'A' && path[0U] <= L'Z')
				 || (path[0U] >= L'a' && path[0U] <= L'z'))) {
			return false;
		}
		std::size_t offset = 3U;
		while (offset < path.size()) {
			const auto separator = path.find(L'\\', offset);
			const auto end       = separator == std::wstring_view::npos
				? path.size()
				: separator;
			const auto component = path.substr(offset, end - offset);
			if (component.empty() || component.size() > maximum_component
				|| component == L"." || component == L".."
				|| component.back() == L'.' || component.back() == L' '
				|| component.find(L':') != std::wstring_view::npos
				|| component_is_reserved(component)) {
				return false;
			}
			offset = end + 1U;
		}
		return true;
	}

	[[nodiscard]] inline auto canonical_windows_root(
		const std::filesystem::path& input)
		-> std::expected<std::filesystem::path, failure>
	{
		const auto raw = input.native();
		if (raw.empty() || raw.find(L'/') != std::wstring::npos
			|| raw.find(L'\0') != std::wstring::npos
			|| raw.size() + ReservedCatalogPathCharacters
				>= MaximumWindowsPathCharacters
			|| (raw.size() > 3U && raw.back() == L'\\')
			|| !valid_windows_components(raw)) {
			return std::unexpected(failure::invalid_path);
		}
		const auto required = ::GetFullPathNameW(raw.c_str(), 0U, nullptr, nullptr);
		if (required == 0U
			|| required >= static_cast<DWORD>(MaximumWindowsPathCharacters)) {
			return std::unexpected(failure::invalid_path);
		}
		auto full = std::vector<wchar_t>(
			static_cast<std::size_t>(required) + 1U, L'\0');
		const auto written = ::GetFullPathNameW(
			raw.c_str(), static_cast<DWORD>(full.size()), full.data(), nullptr);
		if (written == 0U || written >= full.size()) {
			return std::unexpected(failure::invalid_path);
		}
		const auto normalized = std::wstring_view{ full.data(), static_cast<std::size_t>(written) };
		if (!same_windows_path(raw, normalized)
			|| !valid_windows_components(normalized)) {
			return std::unexpected(failure::invalid_path);
		}
		auto extended = std::wstring{ L"\\\\?\\" };
		extended.append(normalized);
		if (extended.size() + ReservedCatalogPathCharacters
			>= MaximumWindowsPathCharacters) {
			return std::unexpected(failure::invalid_path);
		}
		return std::filesystem::path{ std::move(extended) };
	}

	[[nodiscard]] inline auto validate_ancestor_chain(
		const std::filesystem::path& path)
		-> std::expected<std::vector<private_windows_object>, failure>
	{
		const auto native = path.native();
		if (!native.starts_with(L"\\\\?\\") || native.size() < 7U) {
			return std::unexpected(failure::invalid_path);
		}
		auto        candidate = std::wstring{ native.substr(0U, 7U) };
		std::size_t offset    = 7U;
		auto        retained  = std::vector<private_windows_object>{ };
		retained.reserve(1U + static_cast<std::size_t>(std::ranges::count(native, L'\\')));
		for (;;) {
			auto opened = open_existing_object(
				std::filesystem::path{ candidate },
				windows_object_kind::directory,
				0U,
				false);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			auto identity = identity_of(
				opened->Value, windows_object_kind::directory);
			if (!identity) {
				return std::unexpected(identity.error());
			}
			if (auto matches = path_matches_handle(
					opened->Value, std::filesystem::path{ candidate });
				!matches) {
				return std::unexpected(matches.error());
			}
			retained.push_back(private_windows_object{
				.Handle   = std::move(*opened),
				.Path     = std::filesystem::path{ candidate },
				.Identity = *identity,
			});
			if (offset >= native.size()) {
				return retained;
			}
			const auto separator = native.find(L'\\', offset);
			const auto end       = separator == std::wstring::npos
				? native.size()
				: separator;
			// The first candidate is the root, `\\?\C:\`, which already ends
			// in the separator. Copying the separator in unconditionally
			// produced `\\?\C:\\Users`, and `\\?\` paths are not normalized,
			// so the handle's own name came back one character shorter and
			// never compared equal.
			if (!candidate.ends_with(L'\\')) {
				candidate.push_back(L'\\');
			}
			candidate.append(native.substr(offset, end - offset));
			offset = end + 1U;
		}
	}

	[[nodiscard]] inline auto open_private_directory(
		const std::filesystem::path& path,
		const private_security&      security,
		const bool                   allow_delete_sharing = true)
		-> std::expected<private_windows_object, failure>
	{
		return open_private_object(
			path,
			windows_object_kind::directory,
			security,
			FILE_LIST_DIRECTORY | FILE_TRAVERSE,
			false,
			allow_delete_sharing);
	}

	[[nodiscard]] inline auto ensure_private_child(
		const private_windows_object& parent,
		const std::wstring_view       name,
		const private_security&       security)
		-> std::expected<private_windows_object, failure>
	{
		const auto                  path = parent.Path / std::wstring{ name };
		private_security_attributes attributes;
		if (!attributes.initialize(security, windows_object_kind::directory)) {
			return std::unexpected(failure::unavailable);
		}
		if (::CreateDirectoryW(path.c_str(), attributes.attributes()) == 0) {
			const auto error = ::GetLastError();
			if (error != ERROR_ALREADY_EXISTS) {
				return std::unexpected(windows_error_failure(error));
			}
		}
		auto result = open_private_directory(path, security, false);
		if (!result || result->Identity.Volume != parent.Identity.Volume) {
			return std::unexpected(
				result ? failure::invalid_path : result.error());
		}
		return result;
	}

	[[nodiscard]] inline auto reliable_local_filesystem(
		const private_windows_object& root) -> bool
	{
		const auto native = root.Path.native();
		if (native.size() < 7U) {
			return false;
		}
		const std::array<wchar_t, 4U> drive_root{
			native[4U], L':', L'\\', L'\0'
		};
		if (::GetDriveTypeW(drive_root.data()) != DRIVE_FIXED) {
			return false;
		}
		DWORD                    serial            = 0U;
		DWORD                    maximum_component = 0U;
		DWORD                    flags             = 0U;
		std::array<wchar_t, 32U> filesystem{ };
		if (::GetVolumeInformationByHandleW(
				root.Handle.Value,
				nullptr,
				0U,
				&serial,
				&maximum_component,
				&flags,
				filesystem.data(),
				static_cast<DWORD>(filesystem.size()))
				== 0
			|| serial == 0U || maximum_component == 0U
			|| (flags & FILE_PERSISTENT_ACLS) == 0U
			|| (flags & FILE_CASE_PRESERVED_NAMES) == 0U
			|| (flags & FILE_UNICODE_ON_DISK) == 0U
			|| (flags & FILE_SUPPORTS_OPEN_BY_FILE_ID) == 0U
			|| (flags & FILE_READ_ONLY_VOLUME) != 0U
			|| !valid_windows_components(
				std::wstring_view{ native }.substr(4U), maximum_component)) {
			return false;
		}
		const auto name = std::wstring_view{ filesystem.data() };
		return same_windows_path(name, L"NTFS")
			|| same_windows_path(name, L"ReFS");
	}

}
#endif
#endif
