// The stores refuse a root another local principal can reach, and the root
// is the one directory the adapter does not create, so the caller has to
// narrow it — in the adapter's own terms. On Windows the first Qt-graph run
// found that no caller did: the application prepared its data root with
// mode 0700, which a DACL cannot see, and the product refused its own
// directory in every mode (BPR-0436). This proves the repair through the
// public path: a root with the entries a fresh directory inherits is
// refused, the same root narrowed by `make_storage_root_owner_private` is
// accepted, and narrowing twice changes nothing.
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

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
	expect(
		!sqlite_adapter::make_storage_root_owner_private(root / "missing"),
		"a directory that does not exist is refused rather than created");

	std::error_code ignored;
	(void)std::filesystem::remove_all(root, ignored);
	std::cout << "storage-root-tests:pass\n";
	return EXIT_SUCCESS;
}
