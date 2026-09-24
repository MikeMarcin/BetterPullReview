#ifndef BPR_PLATFORM_WINDOWS_PRIVATE_OBJECT_HPP
#define BPR_PLATFORM_WINDOWS_PRIVATE_OBJECT_HPP

// [[BPR-0825]]. The one definition of a Windows owner-private object: the
// protected, owner-explicit descriptor a private file or directory is created
// with, and the exact check that an existing object carries it.
//
// The SQLite and logging storage roots (through `src/adapters/native`) and the
// agent's workspace guard, which the Git adapter's workspace journal uses, all
// build and read their objects with it. The Git adapter admits the application
// data root the SQLite adapter created only if it carries this descriptor, so
// a change here moves both sides at once, and neither can silently refuse
// what the other made.
//
// Header-only. It holds no adapter state and no Qt, provider, file-system
// policy or process decision beyond the descriptor's shape. Callers resolve
// the principals, open the object and map a refusal into their own vocabulary.

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
	#include <aclapi.h>

	#include <cstddef>
	#include <cstdint>
	#include <limits>
	#include <utility>
	#include <vector>

namespace bpr::platform::windows_private_object {
	enum class object_kind : std::uint8_t {
		regular_file,
		directory,
	};

	/// A directory's grants pass to what is created inside it; a file's do not.
	[[nodiscard]] constexpr auto inheritance(const object_kind aKind) noexcept -> BYTE
	{
		return aKind == object_kind::directory
			? static_cast<BYTE>(OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)
			: static_cast<BYTE>(0U);
	}

	/// The protected descriptor: the token user owns the object and holds the
	/// one full-access grant, and the process's AppContainer holds a second one
	/// when it runs in one. A lowbox access check refuses an object whose DACL
	/// names only the token user, so without that entry an owner-private object
	/// would exclude its own creator. The owner is set explicitly because an
	/// elevated token otherwise defaults new objects to its TokenOwner (often
	/// BUILTIN\Administrators), which the check below then refuses for an object
	/// this process created itself.
	///
	/// The descriptor points at the caller's SIDs, which must outlive it
	/// ([[BPR-1030]]). A move keeps the descriptor pointing at its own ACL.
	class descriptor final
	{
	public:
		descriptor() noexcept                            = default;
		descriptor(const descriptor&)                    = delete;
		auto operator=(const descriptor&) -> descriptor& = delete;
		descriptor(descriptor&& aOther) noexcept
			: Acl{ std::move(aOther.Acl) }
			, Descriptor{ aOther.Descriptor }
			, Attributes{ aOther.Attributes }
		{
			rebind();
		}
		auto operator=(descriptor&&) -> descriptor& = delete;
		~descriptor()                               = default;

		/// \p aContainer is null unless the process runs in an AppContainer.
		[[nodiscard]] auto initialize(const PSID aUser, const PSID aContainer, const object_kind aKind) -> bool
		{
			if (aUser == nullptr || ::IsValidSid(aUser) == FALSE
				|| (aContainer != nullptr && ::IsValidSid(aContainer) == FALSE)) {
				return false;
			}
			auto required = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)
				+ static_cast<std::size_t>(::GetLengthSid(aUser));
			if (aContainer != nullptr) {
				required += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)
					+ static_cast<std::size_t>(::GetLengthSid(aContainer));
			}
			if (required > std::numeric_limits<DWORD>::max()) {
				return false;
			}
			Acl               = std::vector<std::byte>(required);
			auto* const acl   = reinterpret_cast<PACL>(Acl.data());
			const auto  flags = inheritance(aKind);
			if (::InitializeAcl(acl, static_cast<DWORD>(Acl.size()), ACL_REVISION) == FALSE
				|| ::AddAccessAllowedAceEx(acl, ACL_REVISION, flags, FILE_ALL_ACCESS, aUser) == FALSE
				|| (aContainer != nullptr
					&& ::AddAccessAllowedAceEx(acl, ACL_REVISION, flags, FILE_ALL_ACCESS, aContainer) == FALSE)
				|| ::InitializeSecurityDescriptor(&Descriptor, SECURITY_DESCRIPTOR_REVISION) == FALSE
				|| ::SetSecurityDescriptorOwner(&Descriptor, aUser, FALSE) == FALSE
				|| ::SetSecurityDescriptorDacl(&Descriptor, TRUE, acl, FALSE) == FALSE
				|| ::SetSecurityDescriptorControl(&Descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED) == FALSE) {
				return false;
			}
			Attributes = SECURITY_ATTRIBUTES{
				.nLength              = sizeof(SECURITY_ATTRIBUTES),
				.lpSecurityDescriptor = &Descriptor,
				.bInheritHandle       = FALSE,
			};
			return true;
		}

		[[nodiscard]] auto attributes() noexcept -> SECURITY_ATTRIBUTES* { return &Attributes; }
		[[nodiscard]] auto acl() noexcept -> PACL
		{
			return Acl.empty() ? nullptr : reinterpret_cast<PACL>(Acl.data());
		}

	private:
		void rebind() noexcept
		{
			if (!Acl.empty()) {
				Descriptor.Dacl = reinterpret_cast<PACL>(Acl.data());
			}
			Attributes.lpSecurityDescriptor = &Descriptor;
		}

		std::vector<std::byte> Acl;
		SECURITY_DESCRIPTOR    Descriptor{ };
		SECURITY_ATTRIBUTES    Attributes{ };
	};

	enum class verdict : std::uint8_t {
		/// Exactly the descriptor above.
		owner_private,
		/// Readable, and not that descriptor.
		not_private,
		/// The descriptor could not be read; `Error` says why.
		unreadable,
	};

	struct validation
	{
		verdict Verdict = verdict::unreadable;
		DWORD   Error   = ERROR_SUCCESS;
	};

	/// Reads the owner and DACL from the opened object, so a same-path
	/// substitute cannot pass. The object is owner-private only as exactly the
	/// descriptor above: the token user owns it, the DACL is protected, and it
	/// holds one full-access grant to the user, plus one to the AppContainer
	/// when \p aContainer is not null, with the kind's inheritance. Any other
	/// trustee, type, mask or flag refuses it. The order of the entries is not
	/// part of the contract.
	///
	/// Where the caller allows it, an unprotected DACL passes only as exactly
	/// those entries inherited from a parent: a file's carry no further
	/// inheritance, and a directory's pass the same inheritance to its children
	/// ([[BPR-1038]]). Its access is the parent's, so it is private exactly when
	/// the parent is.
	[[nodiscard]] inline auto validate(
		const HANDLE      aObject,
		const PSID        aUser,
		const PSID        aContainer,
		const object_kind aKind,
		const bool        aAllowInherited = false) -> validation
	{
		PSID                 owner      = nullptr;
		PACL                 dacl       = nullptr;
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		const auto           status     = ::GetSecurityInfo(
			aObject, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl, nullptr, &descriptor);
		struct local_free
		{
			PSECURITY_DESCRIPTOR Value;
			~local_free()
			{
				if (Value != nullptr) {
					static_cast<void>(::LocalFree(Value));
				}
			}
		} const freed{ descriptor };
		if (status != ERROR_SUCCESS) {
			return { .Verdict = verdict::unreadable, .Error = status };
		}
		const auto                  refused  = validation{ .Verdict = verdict::not_private, .Error = ERROR_SUCCESS };
		SECURITY_DESCRIPTOR_CONTROL control  = 0U;
		DWORD                       revision = 0U;
		ACL_SIZE_INFORMATION        information{ };
		const auto                  expectedCount = static_cast<DWORD>(aContainer != nullptr ? 2U : 1U);
		if (aUser == nullptr || owner == nullptr || dacl == nullptr
			|| ::EqualSid(owner, aUser) == FALSE
			|| ::GetSecurityDescriptorControl(descriptor, &control, &revision) == FALSE
			|| (control & SE_DACL_PRESENT) == 0U
			|| (!aAllowInherited && (control & SE_DACL_PROTECTED) == 0U)
			|| ::GetAclInformation(dacl, &information, sizeof(information), AclSizeInformation) == FALSE
			|| information.AceCount != expectedCount) {
			return refused;
		}
		const auto flags         = inheritance(aKind);
		const auto daclProtected = (control & SE_DACL_PROTECTED) != 0U;
		const auto inherited     = aAllowInherited && !daclProtected;
		auto       users         = 0U;
		auto       packages      = 0U;
		for (auto index = DWORD{ 0U }; index < expectedCount; ++index) {
			void* entry = nullptr;
			if (::GetAce(dacl, index, &entry) == FALSE || entry == nullptr) {
				return refused;
			}
			const auto* const ace            = static_cast<const ACCESS_ALLOWED_ACE*>(entry);
			const auto        exactProtected = daclProtected && ace->Header.AceFlags == flags;
			const auto        exactInherited = inherited && ace->Header.AceFlags == static_cast<BYTE>(INHERITED_ACE | flags);
			if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE || ace->Mask != FILE_ALL_ACCESS
				|| (!exactProtected && !exactInherited)) {
				return refused;
			}
			auto* const granted = static_cast<PSID>(const_cast<DWORD*>(&ace->SidStart));
			if (::EqualSid(granted, aUser) != FALSE) {
				++users;
			} else if (aContainer != nullptr && ::EqualSid(granted, aContainer) != FALSE) {
				++packages;
			} else {
				return refused;
			}
		}
		if (users != 1U || packages != (aContainer != nullptr ? 1U : 0U)) {
			return refused;
		}
		return { .Verdict = verdict::owner_private, .Error = ERROR_SUCCESS };
	}
} // namespace bpr::platform::windows_private_object
#endif // defined(_WIN32)

#endif // BPR_PLATFORM_WINDOWS_PRIVATE_OBJECT_HPP
