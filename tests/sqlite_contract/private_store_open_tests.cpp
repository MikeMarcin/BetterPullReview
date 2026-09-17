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

#include <sddl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <new>
#include <string>
#include <string_view>
#include <vector>

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
		bool                  Substituted     = false;
		bool                  Restored        = false;
		DWORD                 SubstituteError = 0U;
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
			const auto created     = Course.Kind == substitution::symbolic_link
				? ::CreateSymbolicLinkW(aPath.c_str(), Course.Victim.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0
				: ::CreateHardLinkW(aPath.c_str(), Course.Victim.c_str(), nullptr) != FALSE;
			Course.Substituted     = created;
			Course.SubstituteError = created ? 0U : ::GetLastError();
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

		// [[BPR-0841]]. A symbolic link needs Developer Mode or the
		// create-symbolic-link privilege. Without either, this course cannot run
		// on the host, and it says so by name instead of failing.
		if (aKind == substitution::symbolic_link && !Course.Substituted
			&& Course.SubstituteError == ERROR_PRIVILEGE_NOT_HELD) {
			std::error_code ignored;
			std::filesystem::rename(Course.Saved, file, ignored);
			std::cout << "private-store-open:" << aLabel << ":skipped:no-symbolic-link-privilege\n";
			return;
		}
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

	// [[BPR-0841]] item 1. An exception after SQLite has opened the file must
	// still close the connection. On Windows an open connection holds the file
	// without delete sharing, so a rename of the store file shows whether it
	// was released.
	void throw_after_open(const testing::private_store_open_stage aStage, const std::filesystem::path&)
	{
		if (aStage == testing::private_store_open_stage::opened) {
			throw std::bad_alloc{ };
		}
	}

	void test_an_exception_after_open_releases_the_file()
	{
		const auto root = scratch_root{ L"bpr-0841-exception" };
		expect(seed_registry(root.Path), "seed the store file");
		const auto file  = root.Path / "state" / "v1" / "profile-registry.sqlite3";
		const auto moved = root.Path / "state" / "v1" / "profile-registry.sqlite3.moved";

		testing::set_private_store_open_checkpoint(throw_after_open);
		auto       store  = adapters::sqlite::make_profile_registry_store(root.Path);
		const auto loaded = store->load({ });
		testing::set_private_store_open_checkpoint(nullptr);
		expect(!loaded.has_value(), "an exception during open is a refusal");

		// The store object is still alive: only the helper can have closed the file.
		std::error_code renameError;
		std::filesystem::rename(file, moved, renameError);
		std::cout << "private-store-open:exception-releases-file:renamed=" << !renameError << '\n';
		expect(!renameError, "the refused open left the store file unheld");
		std::filesystem::rename(moved, file);
		store.reset();
		expect(seed_registry(root.Path), "the store opens normally afterwards");
	}

	// [[BPR-0841]] item 2. A device-binding store that could not admit its file
	// retries on the next call, as it did before the shared open helper, rather
	// than latching the refusal as corruption.
	[[nodiscard]] auto set_file_descriptor(const std::filesystem::path& aPath, const std::wstring& aSddl) -> bool
	{
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(aSddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr) == FALSE) {
			return false;
		}
		const auto set = ::SetFileSecurityW(aPath.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor) != FALSE;
		::LocalFree(descriptor);
		return set;
	}

	[[nodiscard]] auto current_user_sid() -> std::wstring
	{
		auto token = HANDLE{ nullptr };
		expect(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) != FALSE, "open the process token");
		auto size = DWORD{ 0U };
		static_cast<void>(::GetTokenInformation(token, TokenUser, nullptr, 0U, &size));
		auto buffer = std::vector<BYTE>(size);
		expect(::GetTokenInformation(token, TokenUser, buffer.data(), size, &size) != FALSE, "read the token user");
		::CloseHandle(token);
		auto* text = LPWSTR{ nullptr };
		expect(::ConvertSidToStringSidW(static_cast<TOKEN_USER*>(static_cast<void*>(buffer.data()))->User.Sid, &text) != FALSE, "format the token user");
		auto result = std::wstring{ text };
		::LocalFree(text);
		return result;
	}

	void test_device_binding_admission_refusal_is_retried()
	{
		const auto root = scratch_root{ L"bpr-0841-admission" };
		expect(adapters::sqlite::make_device_binding_store(root.Path)->load({ }).has_value(), "seed the device-binding store");
		const auto file = root.Path / "state" / "v1" / "device-bindings.sqlite3";
		const auto user = current_user_sid();
		// Widened: Everyone may read. The admission rules refuse it.
		expect(set_file_descriptor(file, L"D:P(A;;FA;;;" + user + L")(A;;FR;;;WD)"), "widen the store file");

		auto       store   = adapters::sqlite::make_device_binding_store(root.Path);
		const auto refused = store->load({ });
		expect(!refused.has_value(), "a store file readable by Everyone is refused");

		// Narrowed back to exactly the owner-private shape; the same store retries.
		expect(set_file_descriptor(file, L"D:P(A;;FA;;;" + user + L")"), "narrow the store file again");
		const auto retried = store->load({ });
		std::cout << "private-store-open:admission-retry:refused=" << !refused.has_value()
				  << ":retried=" << retried.has_value() << '\n';
		expect(retried.has_value(), "the same device-binding store opens once its file is admissible again");
	}

	// [[BPR-0841]] item 6. A root whose name is not valid UTF-16 cannot be named
	// to SQLite. The store refuses it; the process does not terminate.
	void test_a_lone_surrogate_root_is_refused_not_fatal()
	{
		const auto root   = scratch_root{ std::wstring{ L"bpr-0841-surrogate-" } + wchar_t{ 0xD800 } };
		const auto loaded = adapters::sqlite::make_profile_registry_store(root.Path)->load({ });
		std::cout << "private-store-open:lone-surrogate-root:loaded=" << loaded.has_value() << '\n';
		expect(!loaded.has_value(), "a root that cannot be encoded as UTF-8 is refused");
	}
}

auto main(const int argc, char** argv) -> int
try {
	// With no argument every course runs; one argument selects a single course.
	// [[BPR-0841]] item 5. Anything else is a usage error, never a pass.
	const auto     only    = argc > 1 ? std::string_view{ argv[1] } : std::string_view{ };
	constexpr auto courses = std::array<std::string_view, 6U>{
		"non-ansi-root", "symbolic-link", "hard-link", "exception-releases-file", "admission-retry", "lone-surrogate-root"
	};
	if (argc > 2 || (!only.empty() && std::find(courses.begin(), courses.end(), only) == courses.end())) {
		std::cerr << "FAILED: unknown course; expected no argument or one of the named courses\n";
		return EXIT_FAILURE;
	}
	if (only.empty() || only == "non-ansi-root") {
		test_non_ansi_root_opens_all_three_stores();
	}
	if (only.empty() || only == "symbolic-link") {
		test_substitution_is_refused(substitution::symbolic_link, "symbolic-link");
	}
	if (only.empty() || only == "hard-link") {
		test_substitution_is_refused(substitution::hard_link, "hard-link");
	}
	if (only.empty() || only == "exception-releases-file") {
		test_an_exception_after_open_releases_the_file();
	}
	if (only.empty() || only == "admission-retry") {
		test_device_binding_admission_refusal_is_retried();
	}
	if (only.empty() || only == "lone-surrogate-root") {
		test_a_lone_surrogate_root_is_refused_not_fatal();
	}
	std::cout << "private-store-open-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& error) {
	std::cerr << "FAILED: " << error.what() << '\n';
	return EXIT_FAILURE;
}
