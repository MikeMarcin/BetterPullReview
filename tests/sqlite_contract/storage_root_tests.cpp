// The stores refuse a root another local principal can reach, and the root
// is the one directory the adapter does not create, so the caller has to
// narrow it — in the adapter's own terms. On Windows the first Qt-graph run
// found that no caller did: the application prepared its data root with
// mode 0700, which a DACL cannot see, and the product refused its own
// directory in every mode (BPR-0436). This proves the repair through the
// public path: a root with the entries a fresh directory inherits is
// refused, the same root narrowed by `make_storage_root_owner_private` is
// accepted, and narrowing twice changes nothing.
//
// On POSIX it also proves how a root that does not exist yet is created
// (BPR-0817): under the user-private-group umask 0002 every folder the call
// makes is exactly 0700, a folder that already existed keeps its mode, and a
// link or a file on the way is refused.
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#if !defined(_WIN32)
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

namespace {
	namespace sqlite_adapter = bpr::adapters::sqlite;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto catalog_opens(const std::filesystem::path& aRoot) -> bool
	{
		return sqlite_adapter::make_epoch_catalog({ .StorageRoot = aRoot }).has_value();
	}

	// A directory that inherits whatever its parent grants: on Windows the
	// entries of the temporary directory's own DACL, on POSIX mode 0755.
	[[nodiscard]] auto make_inherited_directory(const std::filesystem::path& aParent)
		-> std::filesystem::path
	{
		const auto root = aParent
			/ ("bpr-storage-root-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		std::error_code error;
		expect(
			std::filesystem::create_directories(root, error) && !error,
			"the inherited root is created");
		std::filesystem::permissions(
			root,
			std::filesystem::perms::owner_all | std::filesystem::perms::group_read
				| std::filesystem::perms::group_exec | std::filesystem::perms::others_read
				| std::filesystem::perms::others_exec,
			std::filesystem::perm_options::replace,
			error);
		expect(!error, "the inherited root keeps a wider-than-owner mode");
		return std::filesystem::canonical(root);
	}

#if !defined(_WIN32)
	// The permission bits and special bits of the named entry itself, never
	// of what a link names.
	[[nodiscard]] auto mode_of(const std::filesystem::path& aPath) -> mode_t
	{
		struct stat status{ };
		expect(::lstat(aPath.c_str(), &status) == 0, "the inspected entry exists");
		return status.st_mode & static_cast<mode_t>(07777);
	}

	[[nodiscard]] auto exists_here(const std::filesystem::path& aPath) -> bool
	{
		std::error_code error;
		return std::filesystem::exists(std::filesystem::symlink_status(aPath, error));
	}

	class scoped_umask final
	{
	public:
		explicit scoped_umask(const mode_t aMask) noexcept
			: Previous{ ::umask(aMask) }
		{
		}
		scoped_umask(const scoped_umask&)                    = delete;
		auto operator=(const scoped_umask&) -> scoped_umask& = delete;
		~scoped_umask() { static_cast<void>(::umask(Previous)); }

	private:
		mode_t Previous;
	};

	void prove_fresh_root_creation(const std::filesystem::path& aParent)
	{
		const auto scratch = std::filesystem::canonical(aParent)
			/ ("bpr-storage-root-create-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		expect(std::filesystem::create_directory(scratch), "the creation scratch is new");
		std::filesystem::permissions(scratch, std::filesystem::perms::owner_all);

		const auto userPrivateGroupMask = scoped_umask{ 0002 };
		{
			// What a Linux desktop login with a user-private group creates.
			const auto      probe = scratch / "umask-probe";
			std::error_code error;
			expect(std::filesystem::create_directory(probe, error) && !error, "the umask probe is created");
			expect(mode_of(probe) == 0775, "the umask would leave an ordinary new folder group-writable");
		}

		const auto fresh = scratch / "fresh";
		const auto a     = fresh / "a";
		const auto b     = a / "b";
		expect(sqlite_adapter::create_storage_root_owner_private(b), "a missing root and its missing ancestors are created");
		expect(mode_of(fresh) == 0700, "the outermost created ancestor is exactly 0700 under umask 0002");
		expect(mode_of(a) == 0700, "the created parent of the root is exactly 0700 under umask 0002");
		expect(mode_of(b) == 0700, "the created root is exactly 0700 under umask 0002");
		expect(catalog_opens(b), "the catalog accepts the created root");

		expect(sqlite_adapter::create_storage_root_owner_private(b), "an existing private root is accepted again");
		expect(mode_of(fresh) == 0700 && mode_of(a) == 0700 && mode_of(b) == 0700,
			   "accepting an existing private root changes no mode");
		expect(catalog_opens(b), "the catalog still accepts the root after the second call");

		// A folder that already existed is somebody else's decision: it is
		// neither repaired as an ancestor nor admitted as a root.
		const auto shared = scratch / "shared";
		expect(std::filesystem::create_directory(shared), "the pre-existing shared folder is created");
		std::filesystem::permissions(shared, static_cast<std::filesystem::perms>(0775), std::filesystem::perm_options::replace);
		expect(mode_of(shared) == 0775, "the pre-existing shared folder starts group-writable");
		expect(sqlite_adapter::create_storage_root_owner_private(shared / "c" / "d"), "missing children of a shared folder are created");
		expect(mode_of(shared) == 0775, "a pre-existing group-writable ancestor is left as it was");
		expect(mode_of(shared / "c") == 0700 && mode_of(shared / "c" / "d") == 0700,
			   "the children created below it are exactly 0700");
		expect(!sqlite_adapter::create_storage_root_owner_private(shared), "a pre-existing shared root is refused");
		expect(mode_of(shared) == 0775, "a refused pre-existing root is not repaired");

		const auto target = scratch / "target";
		expect(std::filesystem::create_directory(target), "the link target is created");
		std::filesystem::permissions(target, std::filesystem::perms::owner_all);
		std::filesystem::create_directory_symlink(target, scratch / "link");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "link" / "child"), "a symbolic-link ancestor is refused");
		expect(!exists_here(target / "child"), "nothing is created through the link");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "link"), "a root that is a symbolic link is refused");
		std::filesystem::create_directory_symlink(scratch / "nowhere", scratch / "dangling");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "dangling" / "child"), "a dangling symbolic-link ancestor is refused");
		expect(!exists_here(scratch / "nowhere"), "the dangling link's target is not created");

		{
			std::ofstream file{ scratch / "file" };
			file << "not a directory";
		}
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "file" / "child"), "a regular-file ancestor is refused");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "file"), "a root that is a regular file is refused");

		expect(!sqlite_adapter::create_storage_root_owner_private(std::filesystem::path{ "relative" } / "root"), "a relative root is refused");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "fresh" / ".." / "spelled"), "a root that is not lexically normal is refused");
		expect(!exists_here(scratch / "spelled"), "a refused spelling creates nothing");
		expect(!sqlite_adapter::create_storage_root_owner_private(scratch / "trailing" / ""), "a root spelled with a trailing separator is refused");
		expect(!exists_here(scratch / "trailing"), "a trailing-separator spelling creates nothing");

		std::error_code ignored;
		(void)std::filesystem::remove_all(scratch, ignored);
	}
#endif
} // namespace

auto main(int aArgumentCount, char** aArguments) -> int
{
	expect(aArgumentCount == 2, "a writable scratch parent directory is the only argument");
	const auto root = make_inherited_directory(std::filesystem::path{ aArguments[1] });

	expect(!catalog_opens(root), "the catalog refuses a root another principal can reach");
	expect(
		sqlite_adapter::make_storage_root_owner_private(root),
		"the root narrows to the adapter's owner-private shape");
	expect(catalog_opens(root), "the catalog accepts the narrowed root");
	expect(
		sqlite_adapter::make_storage_root_owner_private(root),
		"narrowing again is accepted and changes nothing the catalog can see");
	expect(catalog_opens(root), "the catalog still accepts the root after the second narrowing");
	// Qt spells the application's data root with forward slashes, and that
	// spelling is what the application hands over; the directory is the same.
	expect(
		sqlite_adapter::make_storage_root_owner_private(
			std::filesystem::path{ root.generic_string() }),
		"a forward-slash spelling of the same root is accepted");
	expect(
		!sqlite_adapter::make_storage_root_owner_private(root / "missing"),
		"a directory that does not exist is refused rather than created");

	std::error_code ignored;
	(void)std::filesystem::remove_all(root, ignored);
#if !defined(_WIN32)
	prove_fresh_root_creation(std::filesystem::path{ aArguments[1] });
#endif
	std::cout << "storage-root-tests:pass\n";
	return EXIT_SUCCESS;
}
