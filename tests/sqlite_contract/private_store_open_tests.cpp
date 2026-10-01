// [[BPR-0813]], [[BPR-0883]]. Contract for the private SQLite stores under
// `state/v1`.
//
// 1. (Windows) A store file substituted between admission and open is
//    refused. The adapter's test checkpoint runs after the file is admitted and
//    after SQLite has opened it. A symbolic link substituted at the first point
//    and removed at the second leaves the name exactly as admitted when the
//    path-based restriction looks, while SQLite holds another file; only
//    binding the held handle to the name refuses it. A hard link cannot be
//    undone while SQLite holds the file open without delete sharing, so its
//    course shows the refusal but not the gap.
// 2. Every durable store opens under a root whose name lies outside the ANSI
//    code page. [[BPR-0813]] covered the three stores [[BPR-0806]] admitted;
//    [[BPR-0883]] routes the other five through the same open helper and
//    extends the course to every store factory, on every platform. Only on
//    Windows can the name fail: POSIX names are bytes.
// 3. (POSIX) Every store file is owner-only. The course runs under umask 0,
//    so only the stores themselves can narrow what SQLite creates, and it
//    fails for any store file created at SQLite's own default mode (0644).
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/adapters/sqlite/release_update_store.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#if defined(_WIN32)
	#include <windows.h>

	#include <sddl.h>
#else
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	namespace testing = adapters::sqlite::detail::testing;

	using bpr::tests::expect;

	class scratch_root final
	{
	public:
		explicit scratch_root(std::filesystem::path aName)
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation.
			Path = bpr::tests::make_scratch_directory(aName);
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "scratch root is owner-private");
			made.release();
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

	// Every durable store's file under `state/v1`: the per-profile stores and
	// the shared ones, the same eleven the release maintenance files name.
	constexpr auto StoreFiles = std::array<std::string_view, 11U>{
		"profile-registry.sqlite3",
		"window-restoration.sqlite3",
		"device-bindings.sqlite3",
		"appearance-settings.sqlite3",
		"workbench-settings.sqlite3",
		"review-queue.sqlite3",
		"review-ledger.sqlite3",
		"review-mutation-recovery.sqlite3",
		"release-update.sqlite3",
		"update-draft-recovery.sqlite3",
		"epoch-catalog.sqlite3",
	};

	// Every store factory, open at once, so a course can look at their files
	// while SQLite holds them. The mutation and submission recovery stores
	// share one file.
	struct every_store final
	{
		std::unique_ptr<application::profile_registry_port>           Registry;
		std::unique_ptr<application::window_review_restoration_port>  Restoration;
		std::unique_ptr<application::device_binding_store_port>       Bindings;
		std::unique_ptr<application::appearance_settings_port>        Appearance;
		std::unique_ptr<application::workbench_settings_port>         Workbench;
		std::unique_ptr<application::queue_enrollment_store_port>     Queue;
		std::unique_ptr<application::review_ledger_store_port>        Ledger;
		std::unique_ptr<application::review_mutation_recovery_port>   Recovery;
		std::unique_ptr<application::review_submission_recovery_port> Submission;
		std::shared_ptr<application::release_update_settings_port>    ReleaseUpdate;
		std::shared_ptr<application::release_update_recovery_port>    ReleaseRecovery;
		std::unique_ptr<application::epoch_catalog_port>              Catalog;
	};

	// Opens every store under `aRoot` and uses it once, which is when the lazy
	// ones create their file. Every store's outcome is printed before any is
	// asserted, so one run names all that failed.
	[[nodiscard]] auto open_every_store(const std::filesystem::path& aRoot, const std::string_view aCourse) -> every_store
	{
		auto       result  = every_store{ };
		auto       refused = std::vector<std::string_view>{ };
		const auto opened  = [&](const std::string_view aStore, const bool aOpened) {
			std::cout << "private-store-open:" << aCourse << ':' << aStore << ":opened=" << aOpened << '\n';
			if (!aOpened) {
				refused.push_back(aStore);
			}
		};
		const auto scope = [&aRoot] {
			return adapters::sqlite::review_mutation_recovery_configuration{
				.StorageRoot = aRoot,
				.Host        = "github.com",
				.Account     = "octo",
				.Repository  = "octo/app",
				.PullRequest = 7,
			};
		};

		result.Registry = adapters::sqlite::make_profile_registry_store(aRoot);
		opened("profile-registry", result.Registry->load({ }).has_value());
		result.Restoration = adapters::sqlite::make_window_review_restoration_store(aRoot);
		opened("window-restoration", result.Restoration->load({ }).has_value());
		result.Bindings = adapters::sqlite::make_device_binding_store(aRoot);
		opened("device-bindings", result.Bindings->load({ }).has_value());

		auto appearance = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = aRoot });
		opened("appearance-settings", appearance.has_value() && (*appearance)->load({ }).has_value());
		if (appearance) {
			result.Appearance = std::move(*appearance);
		}
		auto workbench = adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = aRoot, .Window = { } });
		opened("workbench-settings", workbench.has_value() && (*workbench)->load({ }).has_value());
		if (workbench) {
			result.Workbench = std::move(*workbench);
		}
		result.Queue = adapters::sqlite::make_queue_enrollment_store({ .StorageRoot = aRoot });
		opened("review-queue", result.Queue->load({ }).has_value());
		auto ledger = adapters::sqlite::make_review_ledger_store({
			.StorageRoot = aRoot,
			.Host        = "github.com",
			.Account     = "octo",
			.Repository  = "octo/app",
			.PullRequest = 7,
		});
		opened("review-ledger", ledger.has_value());
		if (ledger) {
			result.Ledger = std::move(*ledger);
		}
		auto recovery = adapters::sqlite::make_review_mutation_recovery_store(scope());
		opened("review-mutation-recovery", recovery.has_value());
		if (recovery) {
			result.Recovery = std::move(*recovery);
		}
		auto submission = adapters::sqlite::make_review_submission_recovery_store(scope());
		opened("review-submission-recovery", submission.has_value());
		if (submission) {
			result.Submission = std::move(*submission);
		}

		result.ReleaseUpdate = adapters::sqlite::make_release_update_store(aRoot);
		opened("release-update", result.ReleaseUpdate->load().has_value());
		result.ReleaseRecovery = adapters::sqlite::make_release_update_recovery_store(aRoot);
		opened("update-draft-recovery", result.ReleaseRecovery->load().has_value());
		auto catalog = adapters::sqlite::make_epoch_catalog({ .StorageRoot = aRoot });
		opened("epoch-catalog", catalog.has_value());
		if (catalog) {
			result.Catalog = std::move(*catalog);
		}

		auto missing = std::vector<std::string_view>{ };
		for (const auto name : StoreFiles) {
			if (!std::filesystem::is_regular_file(aRoot / "state" / "v1" / std::string{ name })) {
				missing.push_back(name);
			}
		}
		auto summary = std::string{ };
		for (const auto name : refused) {
			summary += " refused:" + std::string{ name };
		}
		for (const auto name : missing) {
			summary += " missing:" + std::string{ name };
		}
		expect(refused.empty() && missing.empty(), "every store opens and has its file under state/v1:" + summary);
		return result;
	}

	void test_non_ansi_root_opens_every_store()
	{
		// Japanese and an emoji, as universal character names so no compiler
		// reads them in its own code page. The emoji lies outside every ANSI
		// code page, the ideographs outside the Western one.
		const auto                  root   = scratch_root{ std::filesystem::path{ u8"bpr-0883-\u65E5\u672C\u8A9E-\U0001F600" } };
		[[maybe_unused]] const auto stores = open_every_store(root.Path, "non-ansi-root");
		std::cout << "private-store-open:non-ansi-root:pass\n";
	}

#if !defined(_WIN32)
	[[nodiscard]] auto store_file(const std::string_view aName) -> bool
	{
		return std::ranges::any_of(StoreFiles, [aName](const std::string_view aStore) {
			return aName == aStore || aName == std::string{ aStore } + "-wal" || aName == std::string{ aStore } + "-shm"
				|| aName == std::string{ aStore } + "-journal";
		});
	}

	// The store files and their SQLite siblings in `aDirectory`, in name order.
	[[nodiscard]] auto store_file_names(const std::filesystem::path& aDirectory) -> std::vector<std::string>
	{
		auto names = std::vector<std::string>{ };
		for (const auto& entry : std::filesystem::directory_iterator{ aDirectory }) {
			if (auto name = entry.path().filename().string(); store_file(name)) {
				names.push_back(std::move(name));
			}
		}
		std::ranges::sort(names);
		return names;
	}

	// [[BPR-0883]]. Under umask 0 the only narrowing is the stores' own, so a
	// store file SQLite created at its own default mode stays readable by
	// group and other and is named here. Its `-wal` and `-shm` files take the
	// mode the database file had when they were made.
	void test_every_store_file_is_owner_only()
	{
		const auto previous = ::umask(0);
		auto       wide     = std::string{ };
		auto       seen     = std::size_t{ 0U };
		{
			const auto                  root   = scratch_root{ "bpr-0883-owner-only" };
			[[maybe_unused]] const auto stores = open_every_store(root.Path, "owner-only");
			for (const auto& name : store_file_names(root.Path / "state" / "v1")) {
				const auto  path = root.Path / "state" / "v1" / name;
				struct stat info{ };
				expect(::lstat(path.c_str(), &info) == 0, "a store file can be examined");
				const auto mode = static_cast<unsigned>(info.st_mode & 07777U);
				std::cout << "private-store-open:owner-only:" << name << ':' << std::oct << mode << std::dec << '\n';
				seen += name.ends_with(".sqlite3") ? 1U : 0U;
				if (!S_ISREG(info.st_mode) || mode != 0600U) {
					wide += " " + name;
				}
			}
		}
		static_cast<void>(::umask(previous));
		expect(seen == StoreFiles.size(), "every store file was examined");
		expect(wide.empty(), "every store file is 0600, but not:" + wide);
		std::cout << "private-store-open:owner-only:pass\n";
	}
#endif

#if defined(_WIN32)
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
#endif
}

auto main(const int argc, char** argv) -> int
try {
	// With no argument every course runs; one argument selects a single course.
	// [[BPR-0841]] item 5. Anything else is a usage error, never a pass. A
	// course is known only on the platforms it runs on.
#if defined(_WIN32)
	constexpr auto courses = std::array<std::string_view, 6U>{
		"non-ansi-root", "symbolic-link", "hard-link", "exception-releases-file", "admission-retry", "lone-surrogate-root"
	};
#else
	constexpr auto courses = std::array<std::string_view, 2U>{ "non-ansi-root", "owner-only" };
#endif
	const auto only = argc > 1 ? std::string_view{ argv[1] } : std::string_view{ };
	if (argc > 2 || (!only.empty() && std::find(courses.begin(), courses.end(), only) == courses.end())) {
		std::cerr << "FAILED: unknown course; expected no argument or one of the named courses\n";
		return EXIT_FAILURE;
	}
	if (only.empty() || only == "non-ansi-root") {
		test_non_ansi_root_opens_every_store();
	}
#if defined(_WIN32)
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
#else
	if (only.empty() || only == "owner-only") {
		test_every_store_file_is_owner_only();
	}
#endif
	std::cout << "private-store-open-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
