#include "../../src/adapters/sqlite/src/storage_internal.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

#if !defined(_WIN32)
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::application;
	using namespace bpr::adapters::sqlite;
	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Path = std::filesystem::temp_directory_path() / ("bpr-0694-roots-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directory(Path);
			expect(make_storage_root_owner_private(Path), "new test root uses canonical private storage policy");
			Path = std::filesystem::canonical(Path);
		}
		~scratch_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
		scratch_directory(const scratch_directory&)                                     = delete;
		auto                  operator=(const scratch_directory&) -> scratch_directory& = delete;
		std::filesystem::path Path;
	};
	[[nodiscard]] auto registry() -> profile_registry_snapshot
	{
		auto result          = profile_registry_snapshot{ };
		result.LastProfileId = 3U;
		result.Profiles.push_back({ { core::bpr_profile_id{ 2U }, "../Work" }, profile_root_mode::isolated, "profile-2" });
		result.Profiles.push_back({ { core::bpr_profile_id{ 3U }, "Work" }, profile_root_mode::isolated, "profile-3" });
		return result;
	}

	void test_roots_are_private_distinct_and_stable()
	{
		const auto scratch  = scratch_directory{ };
		const auto sentinel = scratch.Path / "legacy-domain.fixture";
		{
			std::ofstream out{ sentinel };
			out << "epoch:17; tuned appearance; remembered binding";
		}
		const auto profiles = registry();
		const auto original = *profile_storage_authority::select(profiles, core::default_bpr_profile_id);
		const auto work     = *profile_storage_authority::select(profiles, core::bpr_profile_id{ 2U });
		const auto other    = *profile_storage_authority::select(profiles, core::bpr_profile_id{ 3U });
		expect(resolve_profile_storage_root(scratch.Path, original) == scratch.Path, "Default returns the canonical base in place");
		expect(!std::filesystem::exists(scratch.Path / "state"), "Default root validation creates no state or domain directories");
		const auto workRoot  = resolve_profile_storage_root(scratch.Path, work);
		const auto otherRoot = resolve_profile_storage_root(scratch.Path, other);
		expect(workRoot && *workRoot == scratch.Path / "profiles/profile-2" && otherRoot && *otherRoot == scratch.Path / "profiles/profile-3", "stable IDs derive distinct isolated child roots");
		expect(detail::validate_private_storage_root(*workRoot).has_value() && detail::validate_private_storage_root(*otherRoot).has_value(), "both created roots satisfy the existing native owner-private policy");
		expect(std::filesystem::is_empty(*workRoot) && std::filesystem::is_empty(*otherRoot)
				   && !std::filesystem::exists(scratch.Path / "profiles/state"),
			   "root creation does not initialize domain stores or state beneath the profiles container");
		auto renamed = profiles;
		++renamed.Revision;
		renamed.Profiles[1].Profile.Label = "Other label";
		const auto renamedAuthority       = *profile_storage_authority::select(renamed, core::bpr_profile_id{ 2U });
		expect(resolve_profile_storage_root(scratch.Path, renamedAuthority) == workRoot, "rename and repeated resolution retain the same root");
		std::ifstream in{ sentinel };
		expect(std::string(std::istreambuf_iterator<char>{ in }, { }) == "epoch:17; tuned appearance; remembered binding", "existing legacy data remains byte-for-byte untouched");
	}

	void test_refusal_and_cancellation()
	{
		const auto scratch = scratch_directory{ };
		const auto outside = scratch_directory{ };
		const auto work    = *profile_storage_authority::select(registry(), core::bpr_profile_id{ 2U });
		auto       stop    = std::stop_source{ };
		stop.request_stop();
		const auto cancelled = resolve_profile_storage_root(scratch.Path, work, stop.get_token());
		expect(!cancelled && cancelled.error() == profile_storage_root_failure::cancelled && std::filesystem::is_empty(scratch.Path), "pre-cancelled resolution creates no directories");
		expect(!resolve_profile_storage_root("relative", work), "relative base roots are refused");
		expect(!resolve_profile_storage_root(scratch.Path / ".." / scratch.Path.filename(), work), "traversal spelling cannot hide a changed base or symlink ancestor");
		expect(!resolve_profile_storage_root(scratch.Path / "missing", work), "missing base does not silently create a new environment");
		const auto      container = scratch.Path / "profiles";
		std::error_code linkError;
		std::filesystem::create_directory_symlink(outside.Path, container, linkError);
		if (!linkError) {
			expect(!resolve_profile_storage_root(scratch.Path, work) && std::filesystem::is_empty(outside.Path), "symlinked profiles parent cannot escape the admitted base");
			std::filesystem::remove(container);
			expect(detail::ensure_private_storage_child(scratch.Path, "profiles").has_value(), "safe container is created");
			std::filesystem::create_directory_symlink(outside.Path, container / "profile-2");
			expect(!resolve_profile_storage_root(scratch.Path, work) && std::filesystem::is_empty(outside.Path), "symlinked existing profile root is refused without modifying target");
			std::filesystem::remove(container / "profile-2");
		} else {
#if !defined(_WIN32)
			expect(false, "POSIX symlink fixture must be available");
#else
			std::cout << "profile-storage-root:symlink-fixture-unavailable\n";
#endif
		}
#if !defined(_WIN32)
		std::filesystem::create_directories(container / "profile-2");
		std::filesystem::permissions(container, std::filesystem::perms::owner_all);
		const auto wide = std::filesystem::perms::owner_all | std::filesystem::perms::group_read;
		std::filesystem::permissions(container / "profile-2", wide);
		const auto refused = resolve_profile_storage_root(scratch.Path, work);
		expect(!refused && refused.error() == profile_storage_root_failure::invalid_permissions
				   && std::filesystem::status(container / "profile-2").permissions() == wide,
			   "existing permissive profile root is refused without chmod repair");
		std::filesystem::permissions(scratch.Path, wide);
		expect(!resolve_profile_storage_root(scratch.Path, work) && std::filesystem::status(scratch.Path).permissions() == wide, "existing permissive base is refused unchanged");
		std::filesystem::permissions(scratch.Path, std::filesystem::perms::owner_all);
		if (::geteuid() != 0U) {
			struct stat before{ };
			struct stat after{ };
			expect(::stat("/", &before) == 0, "foreign root metadata is readable");
			expect(!resolve_profile_storage_root("/", work), "foreign-owned existing base is refused");
			expect(::stat("/", &after) == 0 && before.st_uid == after.st_uid && before.st_mode == after.st_mode, "foreign ownership and permissions remain untouched");
		}
#endif
		expect(!detail::ensure_private_storage_child(scratch.Path, "../escape"), "internal child primitive also rejects traversal components");
	}
}

int main()
{
	test_roots_are_private_distinct_and_stable();
	test_refusal_and_cancellation();
	std::cout << "profile-storage-root-tests:pass\n";
	return EXIT_SUCCESS;
}
