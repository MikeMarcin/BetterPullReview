// [[BPR-0806]]. The tests-disabled Windows image could not start its profile
// desktop: Qt's '/' spelling of the default data root reached a storage root
// validator that refuses '/', and once that was converted, the profile registry
// refused its own existing file on POSIX permission bits MSVC always reports as
// shared. This contract holds both halves on the target: the conversion happens
// where a path leaves Qt, the validator stays strict, and an existing store file
// is admitted on the Windows private-object rules and refused when it is not
// private.
#include "../../src/adapters/sqlite/src/storage_internal.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"

#include <QString>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#include <windows.h>

#include <aclapi.h>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class scratch_root final
	{
	public:
		scratch_root()
		{
			const auto name = "bpr-0806-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
			Path            = std::filesystem::canonical(std::filesystem::temp_directory_path()) / name;
			expect(std::filesystem::create_directory(Path), "scratch root is created");
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "scratch root is owner-private");
		}
		~scratch_root()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
		scratch_root(const scratch_root&)                    = delete;
		auto operator=(const scratch_root&) -> scratch_root& = delete;

		std::filesystem::path Path;
	};

	[[nodiscard]] auto qt_spelling(const std::filesystem::path& aPath) -> QString
	{
		return QString::fromStdWString(aPath.native()).replace(QLatin1Char{ '\\' }, QLatin1Char{ '/' });
	}

	[[nodiscard]] auto registry_loads(const std::filesystem::path& aRoot) -> bool
	{
		const auto store = adapters::sqlite::make_profile_registry_store(aRoot);
		return store && store->load({ }).has_value();
	}

	// Adds a read grant for Everyone, leaving the owner's grant in place, so the
	// file stays readable by its owner and is merely no longer private.
	[[nodiscard]] auto share_with_everyone(const std::filesystem::path& aFile) -> bool
	{
		PACL                 current    = nullptr;
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		if (::GetNamedSecurityInfoW(aFile.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &current, nullptr, &descriptor) != ERROR_SUCCESS) {
			return false;
		}
		SID_IDENTIFIER_AUTHORITY world    = SECURITY_WORLD_SID_AUTHORITY;
		PSID                     everyone = nullptr;
		if (::AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone) == 0) {
			::LocalFree(descriptor);
			return false;
		}
		EXPLICIT_ACCESSW grant{ };
		grant.grfAccessPermissions = GENERIC_READ;
		grant.grfAccessMode        = GRANT_ACCESS;
		grant.grfInheritance       = NO_INHERITANCE;
		grant.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
		grant.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
		grant.Trustee.ptstrName    = static_cast<LPWSTR>(everyone);
		PACL widened               = nullptr;
		auto result                = ::SetEntriesInAclW(1, &grant, current, &widened);
		if (result == ERROR_SUCCESS) {
			auto path = std::wstring{ aFile.native() };
			result    = ::SetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, widened, nullptr);
		}
		::LocalFree(widened);
		::FreeSid(everyone);
		::LocalFree(descriptor);
		return result == ERROR_SUCCESS;
	}

	void conversion_happens_where_the_path_leaves_qt()
	{
		const auto qtRoot = QStringLiteral("C:/Users/owner/AppData/Local/BetterPullReview/BetterPullReview");
		const auto host   = app::native_filesystem_path(qtRoot);
		expect(host.native() == L"C:\\Users\\owner\\AppData\\Local\\BetterPullReview\\BetterPullReview",
			   "a Qt forward-slash root becomes the native Windows spelling");
		expect(host.native().find(L'/') == std::wstring::npos, "no forward slash survives the conversion");
	}

	void the_root_validator_stays_strict()
	{
		const auto scratch  = scratch_root{ };
		const auto qtRoot   = qt_spelling(scratch.Path);
		const auto verbatim = std::filesystem::path{ qtRoot.toStdWString() };
		expect(verbatim.native().find(L'/') != std::wstring::npos, "precondition: Qt spells the scratch root with '/'");

		const auto refused = adapters::sqlite::detail::validate_private_storage_root(verbatim);
		expect(!refused && refused.error() == adapters::sqlite::detail::state_directory_failure::invalid_path,
			   "the validator still refuses a forward-slash root");
		expect(adapters::sqlite::detail::validate_private_storage_root(app::native_filesystem_path(qtRoot)).has_value(),
			   "the same root converted at the Qt boundary is admitted");

		const auto authority = application::profile_storage_authority::select(application::profile_registry_snapshot{ }, core::default_bpr_profile_id);
		expect(authority.has_value(), "Default storage authority");
		expect(!adapters::sqlite::resolve_profile_storage_root(verbatim, *authority).has_value(),
			   "profile storage refuses the unconverted root");
		expect(adapters::sqlite::resolve_profile_storage_root(app::native_filesystem_path(qtRoot), *authority).has_value(),
			   "profile storage resolves the converted root");

		const auto unc = std::filesystem::path{ L"\\\\localhost\\C$" } / scratch.Path.relative_path();
		expect(!adapters::sqlite::detail::validate_private_storage_root(unc).has_value(), "a UNC spelling is still refused");
		const auto extended = std::filesystem::path{ L"\\\\?\\" + scratch.Path.native() };
		expect(!adapters::sqlite::detail::validate_private_storage_root(extended).has_value(), "a \\\\?\\ spelling is still refused");
		expect(!adapters::sqlite::detail::validate_private_storage_root(std::filesystem::path{ L"relative\\root" }).has_value(),
			   "a relative root is still refused");
	}

	void an_existing_registry_file_is_admitted_only_while_private()
	{
		const auto scratch = scratch_root{ };
		const auto file    = scratch.Path / "state" / "v1" / "profile-registry.sqlite3";
		expect(registry_loads(scratch.Path), "a fresh registry is created and loaded");
		expect(std::filesystem::is_regular_file(file), "the registry file exists");
		expect(registry_loads(scratch.Path), "a second open admits the registry file it created");
		expect(registry_loads(scratch.Path), "and a third, after its inherited grant was made explicit");

		const auto alias = scratch.Path / "state" / "v1" / "profile-registry-alias.sqlite3";
		expect(::CreateHardLinkW(alias.c_str(), file.c_str(), nullptr) != 0, "a second hard link is made");
		expect(!registry_loads(scratch.Path), "a registry file with two links is refused");
		expect(std::filesystem::remove(alias), "the second link is removed");
		expect(registry_loads(scratch.Path), "the single-link registry file is admitted again");

		expect(share_with_everyone(file), "the registry file is shared with Everyone");
		expect(!registry_loads(scratch.Path), "a registry file with a non-private DACL is refused");
		expect(!registry_loads(scratch.Path), "and it is not repaired by the refusal");
	}
} // namespace

auto main() -> int
{
	conversion_happens_where_the_path_leaves_qt();
	the_root_validator_stays_strict();
	an_existing_registry_file_is_admitted_only_while_private();
	std::cout << "windows-profile-storage-startup-contract:pass\n";
	return EXIT_SUCCESS;
}
