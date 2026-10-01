#if !defined(__linux__)
	#error "The launcher fault shim is Linux-only"
#endif

// [[BPR-1113]]. Preloaded into nested runs of the Linux test launcher by
// `test_network_isolation_runtime_directory`, so that the row can take the
// launcher down failure paths an ordinary run never reaches: a signal that
// arrives just after the row's runtime directory exists, a signal handler that
// cannot be installed, and a wait that fails. The launcher has no hook for
// this. The shim stands in front of three C library calls and passes every
// other use of them through unchanged.
//
// It reads its instructions once, when it is loaded, and then removes them
// and itself from the environment. The launcher hands its environment to the
// child it runs, and the child must run without the shim.
//
// `BPR_TEST_LAUNCHER_FAULT` names the fault: `signal:<number>` raises that
// signal as soon as the launcher's `mkdtemp` has made `/tmp/bpr-run-*`;
// `handlers` refuses the SIGHUP handler; `wait` reaps the child and then
// reports that the wait failed. `BPR_TEST_LAUNCHER_FAULT_RECORD` names a file
// that receives the directory `mkdtemp` made, so the row can look for it.

#include <array>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
	enum class launcher_fault : unsigned char {
		none,
		signal_after_directory,
		handlers_refused,
		wait_refused
	};

	launcher_fault         Fault       = launcher_fault::none;
	int                    FaultSignal = 0;
	std::array<char, 4096> RecordPath{ };

	[[nodiscard]] auto read_fault(const std::string_view aText) -> launcher_fault
	{
		if (aText == "handlers") {
			return launcher_fault::handlers_refused;
		}
		if (aText == "wait") {
			return launcher_fault::wait_refused;
		}
		constexpr auto prefix = std::string_view{ "signal:" };
		if (aText.starts_with(prefix)) {
			const auto number = aText.substr(prefix.size());
			const auto parsed = std::from_chars(number.data(), number.data() + number.size(), FaultSignal);
			if (parsed.ec == std::errc{ } && parsed.ptr == number.data() + number.size() && FaultSignal > 0) {
				return launcher_fault::signal_after_directory;
			}
		}
		return launcher_fault::none;
	}

	[[gnu::constructor]] void read_instructions()
	{
		const auto* fault  = std::getenv("BPR_TEST_LAUNCHER_FAULT");
		const auto* record = std::getenv("BPR_TEST_LAUNCHER_FAULT_RECORD");
		Fault              = read_fault(fault != nullptr ? fault : "");
		if (record != nullptr && std::strlen(record) < RecordPath.size()) {
			std::memcpy(RecordPath.data(), record, std::strlen(record) + 1U);
		}
		(void)::unsetenv("BPR_TEST_LAUNCHER_FAULT");
		(void)::unsetenv("BPR_TEST_LAUNCHER_FAULT_RECORD");
		(void)::unsetenv("LD_PRELOAD");
	}

	template <typename TFunction>
	[[nodiscard]] auto next_definition(const char* aName) -> TFunction*
	{
		return reinterpret_cast<TFunction*>(::dlsym(RTLD_NEXT, aName));
	}

	void record_directory(const char* aDirectory)
	{
		if (RecordPath[0] == '\0') {
			return;
		}
		const auto descriptor = ::open(RecordPath.data(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (descriptor < 0) {
			return;
		}
		// A short or failed write leaves the record incomplete, and the course
		// reading it fails on the directory it cannot find. GCC does not let a
		// void cast discard glibc's warn_unused_result ([[BPR-1262]]).
		[[maybe_unused]] const auto written = ::write(descriptor, aDirectory, std::strlen(aDirectory));
		(void)::close(descriptor);
	}
} // namespace

// The C library's headers declare these three with its own reserved parameter
// names. The definitions keep this tree's names, so the declaration check is
// silenced at each one.

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
extern "C" auto mkdtemp(char* aTemplate) noexcept -> char*
{
	static auto* const next = next_definition<char*(char*)>("mkdtemp");
	auto* const        made = next(aTemplate);
	if (made != nullptr && std::string_view{ made }.contains("/bpr-run-")) {
		record_directory(made);
		if (Fault == launcher_fault::signal_after_directory) {
			(void)::raise(FaultSignal);
		}
	}
	return made;
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
extern "C" auto sigaction(const int aSignal, const struct sigaction* aAction, struct sigaction* aPrevious) noexcept -> int
{
	static auto* const next = next_definition<int(int, const struct sigaction*, struct sigaction*)>("sigaction");
	if (Fault == launcher_fault::handlers_refused && aSignal == SIGHUP && aAction != nullptr) {
		errno = EINVAL;
		return -1;
	}
	return next(aSignal, aAction, aPrevious);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
extern "C" auto waitpid(const pid_t aChild, int* aStatus, const int aOptions) -> pid_t
{
	static auto* const next   = next_definition<pid_t(pid_t, int*, int)>("waitpid");
	const auto         reaped = next(aChild, aStatus, aOptions);
	if (Fault == launcher_fault::wait_refused && reaped > 0) {
		errno = ECHILD;
		return -1;
	}
	return reaped;
}
