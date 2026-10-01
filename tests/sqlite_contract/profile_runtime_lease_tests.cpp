#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

#if !defined(_WIN32)
	#include <csignal>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr::adapters::sqlite;
	using failure = bpr::application::profile_runtime_lease_failure;

	using bpr::tests::expect;

	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Path = std::filesystem::temp_directory_path() / ("bpr-runtime-lease-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			expect(std::filesystem::create_directory(Path), "create scratch");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			expect(make_storage_root_owner_private(Path), "private scratch");
			Path = std::filesystem::canonical(Path);
			made.release();
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

	void test_admission()
	{
		const scratch_directory first;
		const scratch_directory second;
		auto                    owner = acquire_profile_runtime_lease(first.Path);
		expect(owner.has_value(), "admit private root");
		auto duplicate = acquire_profile_runtime_lease(first.Path);
		expect(!duplicate && duplicate.error() == failure::busy, "same-process duplicate is busy");
		expect(acquire_profile_runtime_lease(second.Path).has_value(), "independent root admitted");
		auto retained = *owner;
		owner->reset();
		expect(!acquire_profile_runtime_lease(first.Path), "shared lifetime retains ownership");
		retained.reset();
		expect(acquire_profile_runtime_lease(first.Path).has_value(), "last owner release admits replacement");
		std::stop_source stop;
		stop.request_stop();
		auto cancelled = acquire_profile_runtime_lease(first.Path, stop.get_token());
		expect(!cancelled && cancelled.error() == failure::cancelled, "pre-cancelled admission refused");
		expect(!acquire_profile_runtime_lease("relative"), "relative root refused");
		expect(!acquire_profile_runtime_lease(first.Path / "missing"), "missing root refused");
		expect(!acquire_profile_runtime_lease(first.Path / ".." / first.Path.filename()), "noncanonical spelling refused");
#if !defined(_WIN32)
		std::filesystem::create_directory_symlink(first.Path, second.Path / "alias");
		expect(!acquire_profile_runtime_lease(second.Path / "alias"), "symlink root refused");
		expect(::chmod(first.Path.c_str(), 0755) == 0, "set unsafe permissions");
		expect(!acquire_profile_runtime_lease(first.Path), "nonprivate root refused");
		expect(::chmod(first.Path.c_str(), 0700) == 0, "restore fixture permissions");
#endif
	}

#if !defined(_WIN32)
	void test_process_ownership()
	{
		const scratch_directory root;
		auto                    owner = acquire_profile_runtime_lease(root.Path);
		expect(owner.has_value(), "parent owns profile");
		const auto contender = ::fork();
		expect(contender >= 0, "fork contender");
		if (contender == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] {
				const auto lease = acquire_profile_runtime_lease(root.Path);
				return !lease && lease.error() == failure::busy ? 0 : 1;
			});
		}
		int status = 0;
		expect(::waitpid(contender, &status, 0) == contender && WIFEXITED(status) && WEXITSTATUS(status) == 0, "other process refused busy");
		owner->reset();
		int ready[2]{ };
		expect(::pipe(ready) == 0, "create child readiness pipe");
		const auto child = ::fork();
		expect(child >= 0, "fork owner");
		if (child == 0) {
			// [[BPR-1121]]. The child never unwinds into this process's frames.
			bpr::tests::end_forked_child([&] {
				::close(ready[0]);
				const auto lease  = acquire_profile_runtime_lease(root.Path);
				const char result = lease ? 'y' : 'n';
				// The parent reads this byte; if the write fails it reads end of
				// file and the course fails there ([[BPR-1262]]).
				[[maybe_unused]] const auto written = ::write(ready[1], &result, 1);
				::close(ready[1]);
				for (;;) {
					::pause();
				}
			});
		}
		::close(ready[1]);
		char result = 0;
		expect(::read(ready[0], &result, 1) == 1 && result == 'y', "child acquired ownership");
		::close(ready[0]);
		auto blocked = acquire_profile_runtime_lease(root.Path);
		expect(!blocked && blocked.error() == failure::busy, "child ownership excludes parent");
		expect(::kill(child, SIGKILL) == 0, "terminate fixture owner");
		expect(::waitpid(child, &status, 0) == child && WIFSIGNALED(status), "fixture process died");
		expect(acquire_profile_runtime_lease(root.Path).has_value(), "kernel releases ownership after process death");
	}

	void test_exec_does_not_inherit()
	{
		const scratch_directory root;
		auto                    owner = acquire_profile_runtime_lease(root.Path);
		expect(owner.has_value(), "parent owns profile before exec");
		int ready[2]{ };
		int release[2]{ };
		expect(::pipe(ready) == 0 && ::pipe(release) == 0, "create exec fixture pipes");
		const auto readyArgument   = std::to_string(ready[1]);
		const auto releaseArgument = std::to_string(release[0]);
		const auto child           = ::fork();
		expect(child >= 0, "fork exec fixture");
		if (child == 0) {
			::close(ready[0]);
			::close(release[1]);
			::execl(BPR_LEASE_TEST_EXECUTABLE, BPR_LEASE_TEST_EXECUTABLE, "--exec-child", readyArgument.c_str(), releaseArgument.c_str(), static_cast<char*>(nullptr));
			::_exit(1);
		}
		::close(ready[1]);
		::close(release[0]);
		char signal = 0;
		expect(::read(ready[0], &signal, 1) == 1 && signal == 'y', "child completed exec");
		::close(ready[0]);
		owner->reset();
		const auto replacement = acquire_profile_runtime_lease(root.Path);
		::close(release[1]);
		int status = 0;
		expect(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec fixture exited");
		expect(replacement.has_value(), "live executed child does not retain parent lease");
	}
#endif
}

int main(const int aArgumentCount, char** aArguments)
try {
#if !defined(_WIN32)
	if (aArgumentCount == 4 && std::string_view{ aArguments[1] } == "--exec-child") {
		int        ready         = -1;
		int        release       = -1;
		const auto readyText     = std::string_view{ aArguments[2] };
		const auto releaseText   = std::string_view{ aArguments[3] };
		const auto readyParsed   = std::from_chars(readyText.data(), readyText.data() + readyText.size(), ready);
		const auto releaseParsed = std::from_chars(releaseText.data(), releaseText.data() + releaseText.size(), release);
		if (readyParsed.ec != std::errc{ } || readyParsed.ptr != readyText.data() + readyText.size()
			|| releaseParsed.ec != std::errc{ } || releaseParsed.ptr != releaseText.data() + releaseText.size() || ready < 0 || release < 0) {
			return 1;
		}
		char signal = 'y';
		if (::write(ready, &signal, 1) != 1) {
			return 1;
		}
		::close(ready);
		const auto result = ::read(release, &signal, 1);
		::close(release);
		return result == 0 ? 0 : 1;
	}
#else
	(void)aArgumentCount;
	(void)aArguments;
#endif
	test_admission();
#if !defined(_WIN32)
	test_process_ownership();
	test_exec_does_not_inherit();
#endif
	std::cout << "profile-runtime-lease-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
