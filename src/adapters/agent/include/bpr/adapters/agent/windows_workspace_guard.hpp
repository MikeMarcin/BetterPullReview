#ifndef BPR_ADAPTERS_AGENT_WINDOWS_WORKSPACE_GUARD_HPP
#define BPR_ADAPTERS_AGENT_WINDOWS_WORKSPACE_GUARD_HPP

// [[BPR-0622]]. The Windows half of the workspace lifetime guard: exact file
// identity, a private current-user security descriptor, and a byte-range lock
// that recovery can only take exclusively once every holder is gone.
//
// Measured on Windows 11 before this design was chosen: a LockFileEx lock is
// owned by the process that took it, so a duplicated handle does not keep it
// alive after that process dies. Every holder therefore opens the guard and
// takes its own lock; nothing hands a locked handle to another process.
//
// Header-only and free of Qt, so the standalone supervisor and the Git
// adapter's ownership journal use the same rules.

#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#include <windows.h>

#include <aclapi.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::adapters::agent::detail {
	/// The one byte every holder shares and recovery locks exclusively. Far
	/// above the 64 KiB journal bound, so no journal read, append or flush can
	/// ever touch the locked range.
	inline constexpr std::uint64_t windows_workspace_guard_lock_offset = 0x4000'0000'0000'0000ULL;

	/// Kind 1 names a regular file, as in the Git adapter's opaque workspace
	/// identities (kind 2 is a directory); the remaining words are the volume
	/// serial and the high and low file index, where Unix records st_dev and
	/// st_ino.
	inline constexpr std::uint64_t windows_file_identity_kind = 1U;

	class windows_handle final
	{
	public:
		windows_handle() noexcept = default;
		explicit windows_handle(const HANDLE aHandle) noexcept
			: Value{ aHandle == INVALID_HANDLE_VALUE ? nullptr : aHandle }
		{
		}
		windows_handle(const windows_handle&)                    = delete;
		auto operator=(const windows_handle&) -> windows_handle& = delete;
		windows_handle(windows_handle&& aOther) noexcept
			: Value{ std::exchange(aOther.Value, nullptr) }
		{
		}
		auto operator=(windows_handle&& aOther) noexcept -> windows_handle&
		{
			if (this != &aOther) {
				reset();
				Value = std::exchange(aOther.Value, nullptr);
			}
			return *this;
		}
		~windows_handle() { reset(); }

		[[nodiscard]] auto get() const noexcept -> HANDLE { return Value; }
		explicit           operator bool() const noexcept { return Value != nullptr; }
		void               reset(const HANDLE aHandle = nullptr) noexcept
		{
			if (Value != nullptr) {
				static_cast<void>(::CloseHandle(Value));
			}
			Value = aHandle == INVALID_HANDLE_VALUE ? nullptr : aHandle;
		}

	private:
		HANDLE Value = nullptr;
	};

	/// The process token's user, and its AppContainer SID when contained. A
	/// contained process can only open objects that also grant its package.
	class windows_private_principals final
	{
	public:
		[[nodiscard]] static auto load() -> std::optional<windows_private_principals>
		{
			HANDLE token = nullptr;
			if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) == FALSE) {
				return std::nullopt;
			}
			const auto owned  = windows_handle{ token };
			auto       result = windows_private_principals{ };
			if (!query(token, TokenUser, result.User)) {
				return std::nullopt;
			}
			DWORD contained = 0U;
			DWORD size      = 0U;
			if (::GetTokenInformation(token, TokenIsAppContainer, &contained, sizeof(contained), &size) != FALSE && contained != 0U) {
				if (!query(token, TokenAppContainerSid, result.Container)) {
					return std::nullopt;
				}
			}
			return result;
		}

		[[nodiscard]] auto user() const noexcept -> PSID
		{
			return static_cast<TOKEN_USER*>(static_cast<void*>(const_cast<BYTE*>(User.data())))->User.Sid;
		}
		[[nodiscard]] auto container() const noexcept -> PSID
		{
			return Container.empty()
				? nullptr
				: static_cast<TOKEN_APPCONTAINER_INFORMATION*>(static_cast<void*>(const_cast<BYTE*>(Container.data())))->TokenAppContainer;
		}

	private:
		[[nodiscard]] static auto query(const HANDLE aToken, const TOKEN_INFORMATION_CLASS aClass, std::vector<BYTE>& aBuffer) -> bool
		{
			DWORD size = 0U;
			static_cast<void>(::GetTokenInformation(aToken, aClass, nullptr, 0U, &size));
			if (size == 0U || size > 64U * 1024U) {
				return false;
			}
			aBuffer.resize(size);
			return ::GetTokenInformation(aToken, aClass, aBuffer.data(), size, &size) != FALSE;
		}

		std::vector<BYTE> User;
		std::vector<BYTE> Container;
	};

	/// Builds the protected, owner-explicit descriptor every private workspace
	/// object is created with: owner and one full-access grant for the token
	/// user, plus the AppContainer when contained. The owner is set explicitly
	/// because an elevated token otherwise defaults new objects to its
	/// TokenOwner (often BUILTIN\Administrators), which would then fail the
	/// check below for an object this process created itself.
	class windows_private_descriptor final
	{
	public:
		[[nodiscard]] static auto make(const windows_private_principals& aPrincipals, const bool aDirectory)
			-> std::optional<windows_private_descriptor>
		{
			auto       result = windows_private_descriptor{ };
			const auto flags  = static_cast<BYTE>(aDirectory ? (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0U);
			auto       size   = static_cast<DWORD>(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + ::GetLengthSid(aPrincipals.user()));
			if (aPrincipals.container() != nullptr) {
				size += static_cast<DWORD>(sizeof(ACCESS_ALLOWED_ACE) + ::GetLengthSid(aPrincipals.container()));
			}
			result.Acl.resize(size);
			const auto acl = static_cast<PACL>(static_cast<void*>(result.Acl.data()));
			if (::InitializeAcl(acl, size, ACL_REVISION) == FALSE
				|| ::AddAccessAllowedAceEx(acl, ACL_REVISION, flags, FILE_ALL_ACCESS, aPrincipals.user()) == FALSE
				|| (aPrincipals.container() != nullptr
					&& ::AddAccessAllowedAceEx(acl, ACL_REVISION, flags, FILE_ALL_ACCESS, aPrincipals.container()) == FALSE)
				|| ::InitializeSecurityDescriptor(&result.Descriptor, SECURITY_DESCRIPTOR_REVISION) == FALSE
				|| ::SetSecurityDescriptorOwner(&result.Descriptor, aPrincipals.user(), FALSE) == FALSE
				|| ::SetSecurityDescriptorDacl(&result.Descriptor, TRUE, acl, FALSE) == FALSE
				|| ::SetSecurityDescriptorControl(&result.Descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED) == FALSE) {
				return std::nullopt;
			}
			result.Attributes.nLength              = sizeof(result.Attributes);
			result.Attributes.lpSecurityDescriptor = &result.Descriptor;
			result.Attributes.bInheritHandle       = FALSE;
			return result;
		}

		windows_private_descriptor(const windows_private_descriptor&)                    = delete;
		auto operator=(const windows_private_descriptor&) -> windows_private_descriptor& = delete;
		windows_private_descriptor(windows_private_descriptor&& aOther) noexcept
			: Acl{ std::move(aOther.Acl) }
			, Descriptor{ aOther.Descriptor }
			, Attributes{ aOther.Attributes }
		{
			rebind();
		}
		auto operator=(windows_private_descriptor&&) -> windows_private_descriptor& = delete;
		~windows_private_descriptor()                                               = default;

		[[nodiscard]] auto attributes() noexcept -> SECURITY_ATTRIBUTES* { return &Attributes; }

	private:
		windows_private_descriptor() noexcept = default;
		void rebind() noexcept
		{
			if (!Acl.empty()) {
				Descriptor.Dacl = static_cast<PACL>(static_cast<void*>(Acl.data()));
			}
			Attributes.lpSecurityDescriptor = &Descriptor;
		}

		std::vector<BYTE>   Acl;
		SECURITY_DESCRIPTOR Descriptor{ };
		SECURITY_ATTRIBUTES Attributes{ };
	};

	/// True only for exactly the descriptor above: owner is the token user,
	/// the DACL is protected, and it holds only full-access grants to the user
	/// (and the AppContainer when contained), non-inheritable on a file. A
	/// private-looking path or an inherited grant is not enough.
	[[nodiscard]] inline auto windows_private_to_current_user(
		const HANDLE aObject, const windows_private_principals& aPrincipals, const bool aDirectory) -> bool
	{
		PSID                 owner      = nullptr;
		PACL                 dacl       = nullptr;
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		if (::GetSecurityInfo(aObject, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl, nullptr, &descriptor) != ERROR_SUCCESS) {
			return false;
		}
		struct local_free
		{
			PSECURITY_DESCRIPTOR Value;
			~local_free() { static_cast<void>(::LocalFree(Value)); }
		} freed{ descriptor };
		SECURITY_DESCRIPTOR_CONTROL control  = 0U;
		DWORD                       revision = 0U;
		if (owner == nullptr || ::EqualSid(owner, aPrincipals.user()) == FALSE || dacl == nullptr
			|| ::GetSecurityDescriptorControl(descriptor, &control, &revision) == FALSE
			|| (control & SE_DACL_PROTECTED) == 0U) {
			return false;
		}
		const auto expectedFlags = static_cast<BYTE>(aDirectory ? (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0U);
		auto       user          = 0U;
		auto       container     = 0U;
		for (DWORD index = 0U; index < dacl->AceCount; ++index) {
			void* entry = nullptr;
			if (::GetAce(dacl, index, &entry) == FALSE) {
				return false;
			}
			const auto* const header = static_cast<ACE_HEADER*>(entry);
			if (header->AceType != ACCESS_ALLOWED_ACE_TYPE || header->AceFlags != expectedFlags) {
				return false;
			}
			const auto* const allowed = static_cast<ACCESS_ALLOWED_ACE*>(entry);
			const auto        sid     = static_cast<PSID>(static_cast<void*>(const_cast<DWORD*>(&allowed->SidStart)));
			if (allowed->Mask != FILE_ALL_ACCESS) {
				return false;
			}
			if (::EqualSid(sid, aPrincipals.user()) != FALSE) {
				++user;
			} else if (aPrincipals.container() != nullptr && ::EqualSid(sid, aPrincipals.container()) != FALSE) {
				++container;
			} else {
				return false;
			}
		}
		return user == 1U && container == (aPrincipals.container() != nullptr ? 1U : 0U);
	}

	/// Kind 2 identity of a single-link regular file that is not a reparse point.
	[[nodiscard]] inline auto windows_regular_file_identity(const HANDLE aFile) -> std::optional<std::array<std::uint64_t, 4U>>
	{
		FILE_ATTRIBUTE_TAG_INFO    tag{ };
		BY_HANDLE_FILE_INFORMATION information{ };
		if (::GetFileInformationByHandleEx(aFile, FileAttributeTagInfo, &tag, sizeof(tag)) == FALSE
			|| (tag.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0U
			|| ::GetFileInformationByHandle(aFile, &information) == FALSE || information.nNumberOfLinks != 1U) {
			return std::nullopt;
		}
		return std::array<std::uint64_t, 4U>{
			windows_file_identity_kind,
			static_cast<std::uint64_t>(information.dwVolumeSerialNumber),
			static_cast<std::uint64_t>(information.nFileIndexHigh),
			static_cast<std::uint64_t>(information.nFileIndexLow),
		};
	}

	enum class windows_guard_lock : std::uint8_t {
		shared,
		exclusive,
	};

	/// Opens the exact private guard and takes this process's own lock on the
	/// fixed range. An absolute path without forward slashes names it; the
	/// open does not follow a reparse point; identity and descriptor are read
	/// from the opened handle, so a same-path substitute cannot pass.
	[[nodiscard]] inline auto open_windows_workspace_guard(
		const std::wstring&                  aPath,
		const std::array<std::uint64_t, 4U>& aIdentity,
		const windows_guard_lock             aLock) -> windows_handle
	{
		if (aPath.size() < 3U || aPath.size() > 32'767U || aPath.find(L'/') != std::wstring::npos
			|| aPath.find(L'\0') != std::wstring::npos || aIdentity[0] != windows_file_identity_kind) {
			return { };
		}
		const auto principals = windows_private_principals::load();
		if (!principals) {
			return { };
		}
		auto guard = windows_handle{ ::CreateFileW(
			aPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
		if (!guard) {
			return { };
		}
		const auto identity = windows_regular_file_identity(guard.get());
		if (!identity || *identity != aIdentity || !windows_private_to_current_user(guard.get(), *principals, false)) {
			return { };
		}
		auto overlapped       = OVERLAPPED{ };
		overlapped.Offset     = static_cast<DWORD>(windows_workspace_guard_lock_offset & 0xFFFF'FFFFULL);
		overlapped.OffsetHigh = static_cast<DWORD>(windows_workspace_guard_lock_offset >> 32U);
		const auto flags      = static_cast<DWORD>(LOCKFILE_FAIL_IMMEDIATELY | (aLock == windows_guard_lock::exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0U));
		if (::LockFileEx(guard.get(), flags, 0U, 1U, 0U, &overlapped) == FALSE) {
			return { };
		}
		return guard;
	}
} // namespace bpr::adapters::agent::detail

#endif // BPR_ADAPTERS_AGENT_WINDOWS_WORKSPACE_GUARD_HPP
