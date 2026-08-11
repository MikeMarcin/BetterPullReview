#include "designated_live_mutation_gate.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#if !defined(_WIN32)
	#include <fcntl.h>
	#include <sys/resource.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::tests::network_isolation {
	namespace {

		constexpr std::string_view ValidCapability = "bpr.designated-live-capability/v1\nsession_nonce=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
		unsigned                   Failures{ };

		void expect(bool aCondition, std::string_view aMessage)
		{
			if (!aCondition) {
				++Failures;
				std::cerr << "FAILED: " << aMessage << '\n';
			}
		}

#if !defined(_WIN32)
		void write_file(const std::filesystem::path& aPath, std::string_view aBody)
		{
			const int descriptor = open(aPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
			expect(descriptor >= 0, "substitution fixture opens");
			if (descriptor < 0) {
				return;
			}
			expect(write(descriptor, aBody.data(), aBody.size()) == static_cast<ssize_t>(aBody.size()), "substitution fixture writes");
			close(descriptor);
		}

		class substitute_unrelated_path final : public fixed_canary_pre_start_hook
		{
		public:
			substitute_unrelated_path(
				std::filesystem::path aPath,
				std::filesystem::path aDisplaced)
				: Path(std::move(aPath))
				, Displaced(std::move(aDisplaced))
			{
			}

			void after_surface_binding() override
			{
				Called = true;
				expect(rename(Path.c_str(), Displaced.c_str()) == 0, "post-binding hook substitutes filesystem path");
				write_file(Path, "replacement executable bytes");
			}

			bool Called{ };

		private:
			std::filesystem::path Path;
			std::filesystem::path Displaced;
		};

		void test_fixed_forked_canary()
		{
			fixed_canary_options options;
			const auto           digestMismatch = start_fixed_canary(
				ValidCapability,
				"0000000000000000000000000000000000000000000000000000000000000000",
				options);
			expect(digestMismatch.Status == child_run_status::not_started, "surface identity mismatch refuses before fork");

			int threadBlocker[2]{ -1, -1 };
			expect(pipe(threadBlocker) == 0, "multithread refusal fixture creates pipe");
			if (threadBlocker[0] >= 0 && threadBlocker[1] >= 0) {
				std::thread extraThread([descriptor = threadBlocker[0]] {
					char value{ };
					while (read(descriptor, &value, sizeof(value)) < 0 && errno == EINTR) {
					}
					close(descriptor);
				});
				const auto  multithreaded = start_fixed_canary(
					ValidCapability,
					designated_live_canary_surface_sha256(),
					options);
				expect(multithreaded.Status == child_run_status::parent_multithreaded, "multithreaded parent refuses before fork");
				expect(multithreaded.ObservedThreadCount >= 2, "multithreaded refusal reports observed count");
				close(threadBlocker[1]);
				extraThread.join();
			}

			std::array<char, 128>      pattern{ };
			constexpr std::string_view prefix = "/tmp/bpr-designated-live-no-path.XXXXXX";
			std::copy(prefix.begin(), prefix.end(), pattern.begin());
			const char* created = mkdtemp(pattern.data());
			expect(created != nullptr, "post-binding substitution directory exists");
			if (created == nullptr) {
				return;
			}
			const std::filesystem::path directory(created);
			const auto                  path      = directory / "canary";
			const auto                  displaced = directory / "displaced";
			write_file(path, "original executable bytes");
			substitute_unrelated_path hook(path, displaced);
			options.Hook           = &hook;
			const auto substituted = start_fixed_canary(
				ValidCapability,
				designated_live_canary_surface_sha256(),
				options);
			if (substituted.Status != child_run_status::succeeded) {
				std::cerr << "CHILD_PRECONDITION_DIAGNOSTIC:status="
						  << static_cast<int>(substituted.Status)
						  << ":threads=" << substituted.ObservedThreadCount
						  << ":first_fds=" << substituted.FirstDescriptorCount
						  << ":second_fds=" << substituted.SecondDescriptorCount << '\n';
			}
			expect(hook.Called, "post-binding hook runs immediately before child creation");
			expect(substituted.Status == child_run_status::succeeded, "filesystem executable substitution cannot affect compiled-in canary");
			static_cast<void>(unlink(path.c_str()));
			static_cast<void>(unlink(displaced.c_str()));
			expect(rmdir(directory.c_str()) == 0, "post-binding substitution fixture cleans exactly");

			const int baseDescriptor = open("/dev/null", O_RDONLY | O_CLOEXEC);
			expect(baseDescriptor >= 0, "high descriptor fixture opens");
			const int highDescriptor = baseDescriptor < 0
				? -1
				: fcntl(baseDescriptor, F_DUPFD_CLOEXEC, 256);
			if (baseDescriptor >= 0) {
				close(baseDescriptor);
			}
			expect(highDescriptor >= 256, "high descriptor fixture reaches at least 256");
			if (highDescriptor >= 256) {
				rlimit originalLimit{ };
				expect(getrlimit(RLIMIT_NOFILE, &originalLimit) == 0, "descriptor limit reads");
				rlimit reducedLimit   = originalLimit;
				reducedLimit.rlim_cur = 64;
				const bool reduced    = originalLimit.rlim_cur >= reducedLimit.rlim_cur
					&& setrlimit(RLIMIT_NOFILE, &reducedLimit) == 0;
				expect(reduced, "descriptor limit lowers beneath inherited high descriptor");
				if (reduced) {
					options                            = { };
					options.ForbiddenDescriptorForTest = highDescriptor;
					const auto highDescriptorResult    = start_fixed_canary(
						ValidCapability,
						designated_live_canary_surface_sha256(),
						options);
					expect(setrlimit(RLIMIT_NOFILE, &originalLimit) == 0, "descriptor limit restores");
					expect(highDescriptorResult.Status == child_run_status::succeeded, "child closes inherited descriptor above lowered limit");
					expect(fcntl(highDescriptor, F_GETFD) >= 0, "child closure does not close parent descriptor");
				}
				close(highDescriptor);
				errno = 0;
				expect(fcntl(highDescriptor, F_GETFD) == -1 && errno == EBADF, "high descriptor fixture leaves no parent leak");
			}

			const auto malformed = start_fixed_canary(
				"bpr.designated-live-capability/v1\nsession_nonce=not-valid\n",
				designated_live_canary_surface_sha256());
			expect(malformed.Status == child_run_status::failed, "malformed stdin protocol fails in fixed child");

			options           = { };
			options.Behavior  = fixed_canary_behavior::exit_failure_after_protocol;
			const auto failed = start_fixed_canary(
				ValidCapability,
				designated_live_canary_surface_sha256(),
				options);
			expect(failed.Status == child_run_status::failed, "fixed child nonzero exit is reported");

			options.Behavior            = fixed_canary_behavior::timeout_after_protocol;
			options.TimeoutMilliseconds = 50;
			const auto startedAt        = std::chrono::steady_clock::now();
			const auto timedOut         = start_fixed_canary(
				ValidCapability,
				designated_live_canary_surface_sha256(),
				options);
			const auto elapsed = std::chrono::steady_clock::now() - startedAt;
			expect(timedOut.Status == child_run_status::timed_out, "fixed child timeout is killed and typed");
			expect(elapsed < std::chrono::seconds(2), "fixed child timeout remains bounded");
		}
#else
		void test_fixed_forked_canary()
		{
			const auto result = start_fixed_canary(
				ValidCapability,
				designated_live_canary_surface_sha256());
			expect(result.Status == child_run_status::not_started, "Windows child creation remains fail closed");
		}
#endif

	} // namespace
} // namespace bpr::tests::network_isolation

int main()
{
	bpr::tests::network_isolation::test_fixed_forked_canary();
	if (bpr::tests::network_isolation::Failures != 0) {
		return 1;
	}
	std::cout << "DESIGNATED_LIVE_RUNNER_CONTRACT_OK\n";
	return 0;
}
