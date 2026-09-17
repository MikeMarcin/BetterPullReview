// [[BPR-0813]]. Windows contract for the three private SQLite stores that
// [[BPR-0806]] admitted: the profile registry, window-review restoration and
// device bindings.
//
// 1. A store file substituted between admission and open is refused. The
//    adapter's test checkpoint runs after the file is admitted and after SQLite
//    has opened it. A symbolic link substituted at the first point and removed
//    at the second leaves the name exactly as admitted when the path-based
//    restriction looks, while SQLite holds another file; only binding the held
//    handle to the name refuses it. A hard link cannot be undone while SQLite
//    holds the file open without delete sharing, so its course shows the
//    refusal but not the gap.
// 2. All three stores open under a root whose name lies outside the ANSI code
//    page.
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	namespace testing = adapters::sqlite::detail::testing;

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
		explicit scratch_root(const std::wstring& aName)
		{
			Path = std::filesystem::canonical(std::filesystem::temp_directory_path())
				/ (aName + L"-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directory(Path);
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

	enum class substitution : std::uint8_t {
		symbolic_link,
		hard_link,
	};

	// The checkpoint is a plain function pointer, so the course it acts for is
	// module state.
	struct armed_course final
	{
		substitution          Kind = substitution::symbolic_link;
		std::filesystem::path Victim;
		std::filesystem::path Saved;
		bool                  Substituted = false;
		bool                  Restored    = false;
	};
	armed_course Course;

	void substitute(const testing::private_store_open_stage aStage, const std::filesystem::path& aPath)
	{
		std::error_code error;
		if (aStage == testing::private_store_open_stage::admitted) {
			std::filesystem::rename(aPath, Course.Saved, error);
			if (error) {
				return;
			}
			const auto created = Course.Kind == substitution::symbolic_link
				? ::CreateSymbolicLinkW(aPath.c_str(), Course.Victim.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0
				: ::CreateHardLinkW(aPath.c_str(), Course.Victim.c_str(), nullptr) != FALSE;
			Course.Substituted = created;
			return;
		}
		if (!Course.Substituted) {
			return;
		}
		// Put the admitted name back before the path-based restriction looks.
		// The symbolic link itself is not what SQLite holds, so it can go.
		if (::DeleteFileW(aPath.c_str()) != FALSE) {
			std::filesystem::rename(Course.Saved, aPath, error);
			Course.Restored = !error;
		}
	}

	[[nodiscard]] auto seed_registry(const std::filesystem::path& aRoot) -> bool
	{
		auto store = adapters::sqlite::make_profile_registry_store(aRoot);
		return store->load({ }).has_value();
	}

	void test_substitution_is_refused(const substitution aKind, const std::string_view aLabel)
	{
		const auto root  = scratch_root{ L"bpr-0813-substitution" };
		const auto other = scratch_root{ L"bpr-0813-victim" };
		expect(seed_registry(root.Path), "seed the store file");
		expect(seed_registry(other.Path), "seed a second, equally private store file");
		const auto file = root.Path / "state" / "v1" / "profile-registry.sqlite3";
		Course          = armed_course{
			.Kind   = aKind,
			.Victim = root.Path / "state" / "v1" / "victim.sqlite3",
			.Saved  = root.Path / "state" / "v1" / "profile-registry.sqlite3.saved",
		};
		// Moved on one volume, the victim keeps the explicit private descriptor
		// its own store wrote, so only the substitution itself can refuse it.
		std::filesystem::rename(other.Path / "state" / "v1" / "profile-registry.sqlite3", Course.Victim);

		testing::set_private_store_open_checkpoint(substitute);
		auto       store  = adapters::sqlite::make_profile_registry_store(root.Path);
		const auto loaded = store->load({ });
		store.reset();
		testing::set_private_store_open_checkpoint(nullptr);

		expect(Course.Substituted, std::string{ "the course substituted a " } + std::string{ aLabel });
		if (aKind == substitution::symbolic_link) {
			expect(Course.Restored, "the admitted name was restored before the restriction");
		}
		std::cout << "private-store-open:" << aLabel << ":restored=" << Course.Restored
				  << ":loaded=" << loaded.has_value() << '\n';
		expect(!loaded.has_value(), std::string{ "a store opened through a substituted " } + std::string{ aLabel } + " is refused");

		// The refusal left the real store usable.
		if (!Course.Restored) {
			std::error_code ignored;
			std::filesystem::remove(file, ignored);
			std::filesystem::rename(Course.Saved, file, ignored);
		}
		expect(seed_registry(root.Path), "the store opens normally once nothing is substituted");
	}

	void test_non_ansi_root_opens_all_three_stores()
	{
		// Japanese and an emoji: neither is representable in a Western, or any
		// single, ANSI code page.
		const auto root = scratch_root{ L"bpr-0813-プロフィール-\U0001F600" };
		expect(adapters::sqlite::make_profile_registry_store(root.Path)->load({ }).has_value(), "profile registry opens under a non-ANSI root");
		expect(adapters::sqlite::make_window_review_restoration_store(root.Path)->load({ }).has_value(), "window-review restoration opens under a non-ANSI root");
		expect(adapters::sqlite::make_device_binding_store(root.Path)->load({ }).has_value(), "device bindings open under a non-ANSI root");
		for (const auto* name : { L"profile-registry.sqlite3", L"window-restoration.sqlite3", L"device-bindings.sqlite3" }) {
			expect(std::filesystem::is_regular_file(root.Path / "state" / "v1" / name), "each store file exists under the non-ANSI root");
		}
		std::cout << "private-store-open:non-ansi-root:pass\n";
	}
}

auto main(const int argc, char** argv) -> int
try {
	// With no argument every course runs; one argument selects a single course.
	const auto only = argc > 1 ? std::string_view{ argv[1] } : std::string_view{ };
	if (only.empty() || only == "non-ansi-root") {
		test_non_ansi_root_opens_all_three_stores();
	}
	if (only.empty() || only == "symbolic-link") {
		test_substitution_is_refused(substitution::symbolic_link, "symbolic-link");
	}
	if (only.empty() || only == "hard-link") {
		test_substitution_is_refused(substitution::hard_link, "hard-link");
	}
	std::cout << "private-store-open-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& error) {
	std::cerr << "FAILED: " << error.what() << '\n';
	return EXIT_FAILURE;
}
