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

		// [[BPR-1868]]. A root made beforehand owner-private is admitted as it is.
		const auto premade = scratch / "premade";
		expect(std::filesystem::create_directory(premade), "the pre-made private root is created");
		std::filesystem::permissions(premade, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
		expect(sqlite_adapter::create_storage_root_owner_private(premade), "a root made beforehand with mode 0700 is accepted");
		expect(catalog_opens(premade), "the catalog accepts the pre-made private root");

		// [[BPR-1868]]. The on-disk spelling: unchanged for a spelling that is
		// already the file system's own, and for one through a link.
		expect(sqlite_adapter::storage_root_on_disk_spelling(b) == b, "a canonical existing root keeps its spelling");
		expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "absent" / "root") == scratch / "absent" / "root",
			   "a missing root under a canonical folder keeps its spelling");
		expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "link" / "child") == scratch / "link" / "child",
			   "a root under a symbolic link keeps its spelling, so it is still refused");
		expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "link") == scratch / "link",
			   "a root that is a symbolic link keeps its spelling");
		expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "fresh" / ".." / "spelled") == scratch / "fresh" / ".." / "spelled",
			   "a spelling that is not lexically normal is returned unchanged");

		// [[BPR-1868]]. On a case-insensitive volume, a root typed in another
		// letter case names the same folder and is given the folder's spelling.
		const auto cased = scratch / "Attended";
		expect(std::filesystem::create_directory(cased), "the cased folder is created");
		if (exists_here(scratch / "attended")) {
			const auto typed = scratch / "attended" / "data";
			expect(!sqlite_adapter::create_storage_root_owner_private(typed), "the stores still refuse a spelling that is not the file system's");
			const auto spelled = sqlite_adapter::storage_root_on_disk_spelling(typed);
			expect(spelled == cased / "data", "a missing root under a folder typed in another case takes the folder's spelling");
			expect(sqlite_adapter::create_storage_root_owner_private(spelled), "the respelled root is created");
			expect(catalog_opens(spelled), "the catalog accepts the respelled root");
			expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "ATTENDED" / "DATA") == cased / "data",
				   "an existing root typed in another case takes its own spelling");
		} else {
			expect(sqlite_adapter::storage_root_on_disk_spelling(scratch / "attended" / "data") == scratch / "attended" / "data",
				   "on a case-sensitive volume another case names another folder and keeps its spelling");
		}

		std::error_code ignored;
		(void)std::filesystem::remove_all(scratch, ignored);
	}

	void loosen(const std::filesystem::path& aPath, const mode_t aMode)
	{
		expect(::chmod(aPath.c_str(), aMode) == 0, "the fixture entry is loosened");
	}

	// [[BPR-1870]]. An existing data root is narrowed only when it is the
	// application's own; a folder with other content keeps its mode and the
	// stores use a private folder made inside it; a link is never followed.
	void prove_existing_root_tightening(const std::filesystem::path& aParent)
	{
		using tightening   = sqlite_adapter::storage_root_tightening;
		const auto scratch = std::filesystem::canonical(aParent)
			/ ("bpr-storage-root-tighten-"
			   + std::to_string(
				   std::chrono::steady_clock::now().time_since_epoch().count()));
		expect(std::filesystem::create_directory(scratch), "the tightening scratch is new");
		std::filesystem::permissions(scratch, std::filesystem::perms::owner_all);

		// What `mkdir -p` gives: empty and 0755.
		const auto empty = scratch / "empty";
		expect(std::filesystem::create_directory(empty), "the empty root is created");
		loosen(empty, 0755);
		expect(sqlite_adapter::storage_root_placement(empty) == empty, "an empty folder is used as the root");
		expect(sqlite_adapter::tighten_existing_storage_root(empty) == tightening::tightened, "an empty 0755 root is narrowed");
		expect(mode_of(empty) == 0700, "the narrowed root is exactly 0700");
		expect(catalog_opens(empty), "the catalog accepts the narrowed root");
		expect(sqlite_adapter::tighten_existing_storage_root(empty) == tightening::already_private, "narrowing again changes nothing");

		// A copied or restored data folder: the application's own entries, all
		// with the modes a copy under umask 022 gives them.
		const auto copied = scratch / "copied";
		expect(std::filesystem::create_directory(copied), "the copied root is created");
		std::filesystem::permissions(copied, std::filesystem::perms::owner_all);
		expect(catalog_opens(copied), "the copied root first holds a catalog");
		const auto version = copied / "state" / "v1";
		const auto catalog = version / "epoch-catalog.sqlite3";
		loosen(copied, 0755);
		loosen(copied / "state", 0755);
		loosen(version, 0755);
		loosen(catalog, 0644);
		expect(!catalog_opens(copied), "the loosened copy is not admitted before it is narrowed");
		expect(sqlite_adapter::storage_root_placement(copied) == copied, "a folder holding state/v1 is used as the root");
		expect(sqlite_adapter::tighten_existing_storage_root(copied) == tightening::tightened, "the copied root is narrowed");
		expect(catalog_opens(copied), "the catalog narrows its own folders and file and opens");
		expect(mode_of(copied) == 0700 && mode_of(copied / "state") == 0700 && mode_of(version) == 0700,
			   "the root, state and v1 are exactly 0700");
		expect(mode_of(catalog) == 0600, "the catalog file is exactly 0600");

		// A folder other programs use: left exactly as it is.
		const auto shared = scratch / "shared";
		expect(std::filesystem::create_directory(shared), "the shared folder is created");
		{
			std::ofstream file{ shared / "notes.txt" };
			file << "another program's file";
		}
		loosen(shared, 0755);
		loosen(shared / "notes.txt", 0644);
		const auto nested = shared / "BetterPullReview" / "data";
		expect(sqlite_adapter::storage_root_placement(shared) == nested, "a folder with other content places the root inside it");
		expect(sqlite_adapter::tighten_existing_storage_root(shared) == tightening::foreign_content, "a folder with other content is not narrowed");
		expect(mode_of(shared) == 0755 && mode_of(shared / "notes.txt") == 0644, "the folder and its file keep their modes");
		expect(sqlite_adapter::create_storage_root_owner_private(nested), "the nested root is created");
		expect(mode_of(shared / "BetterPullReview") == 0700 && mode_of(nested) == 0700, "the nested folders are exactly 0700");
		expect(catalog_opens(nested), "the catalog accepts the nested root");
		expect(mode_of(shared) == 0755, "the shared folder still keeps its mode");
		loosen(shared, 0700);
		expect(sqlite_adapter::storage_root_placement(shared) == nested, "the nested root stays chosen after the folder changes");

		// A link: never followed, its target never changed.
		const auto target = scratch / "target";
		expect(std::filesystem::create_directory(target), "the link target is created");
		loosen(target, 0755);
		std::filesystem::create_directory_symlink(target, scratch / "link");
		expect(sqlite_adapter::storage_root_placement(scratch / "link") == scratch / "link", "a linked root keeps its spelling");
		expect(sqlite_adapter::tighten_existing_storage_root(scratch / "link") == tightening::refused, "a linked root is refused");
		expect(!sqlite_adapter::make_storage_root_owner_private(scratch / "link"), "a linked root is not narrowed by the caller's helper either");
		expect(mode_of(target) == 0755, "the link's target keeps its mode");

		// A store file that is a link to a loose file is refused, and the file
		// it names is not changed.
		const auto linked = scratch / "linked-store";
		expect(std::filesystem::create_directory(linked), "the linked-store root is created");
		std::filesystem::permissions(linked, std::filesystem::perms::owner_all);
		expect(catalog_opens(linked), "the linked-store root first holds a catalog");
		const auto outside = scratch / "outside.sqlite3";
		std::filesystem::rename(linked / "state" / "v1" / "epoch-catalog.sqlite3", outside);
		loosen(outside, 0644);
		std::filesystem::create_symlink(outside, linked / "state" / "v1" / "epoch-catalog.sqlite3");
		expect(!catalog_opens(linked), "a store file that is a link is refused");
		expect(mode_of(outside) == 0644, "the file a store link names keeps its mode");

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
	prove_existing_root_tightening(std::filesystem::path{ aArguments[1] });
#endif
	std::cout << "storage-root-tests:pass\n";
	return EXIT_SUCCESS;
}
