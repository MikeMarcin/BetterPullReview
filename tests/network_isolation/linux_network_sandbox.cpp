#if !defined(__linux__)
	#error "The network namespace launcher is Linux-only"
#endif

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <sched.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
	[[nodiscard]] auto write_text(
		const char* const      aPath,
		const std::string_view aText) -> bool
	{
		const auto descriptor = ::open(aPath, O_WRONLY | O_CLOEXEC);
		if (descriptor < 0) {
			return false;
		}
		auto remaining = aText;
		while (!remaining.empty()) {
			const auto written = ::write(
				descriptor,
				remaining.data(),
				remaining.size());
			if (written <= 0) {
				const auto saved = errno;
				::close(descriptor);
				errno = written == 0 ? EIO : saved;
				return false;
			}
			remaining.remove_prefix(static_cast<std::size_t>(written));
		}
		return ::close(descriptor) == 0;
	}

	// The identity is mapped to itself, not to 0. A user namespace is needed
	// only so that an unprivileged caller may create a network namespace; it is
	// not a request for privilege. Mapping to 0 would hand every test root, and
	// root is exempt from the permission checks the product relies on, so a
	// test asking whether a mode is refused would silently get the wrong
	// answer. Capabilities in the new namespace come from creating it, not from
	// the mapped id, so bringing loopback up still works. No setresuid call
	// follows, both because there is no transition to make and because such a
	// transition is what would drop those capabilities.
	[[nodiscard]] auto map_current_identity(
		const uid_t aUid,
		const gid_t aGid) -> bool
	{
		const auto uidText = std::to_string(aUid);
		const auto gidText = std::to_string(aGid);
		const auto uidMap  = uidText + " " + uidText + " 1\n";
		const auto gidMap  = gidText + " " + gidText + " 1\n";
		if (!write_text("/proc/self/setgroups", "deny\n") && errno != ENOENT) {
			return false;
		}
		return write_text("/proc/self/uid_map", uidMap)
			&& write_text("/proc/self/gid_map", gidMap);
	}

	[[nodiscard]] auto enable_loopback() -> bool
	{
		const auto descriptor = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
		if (descriptor < 0) {
			return false;
		}
		auto           request  = ifreq{ };
		constexpr auto loopback = std::string_view{ "lo" };
		static_assert(loopback.size() < IFNAMSIZ);
		std::memcpy(request.ifr_name, loopback.data(), loopback.size());
		if (::ioctl(descriptor, SIOCGIFFLAGS, &request) != 0) {
			const auto saved = errno;
			::close(descriptor);
			errno = saved;
			return false;
		}
		request.ifr_flags = static_cast<short>(request.ifr_flags | IFF_UP);
		const auto result = ::ioctl(descriptor, SIOCSIFFLAGS, &request);
		const auto saved  = errno;
		::close(descriptor);
		errno = saved;
		return result == 0;
	}

	[[noreturn]] void fail(const std::string_view aStage)
	{
		std::fprintf(
			stderr,
			"BPR_NETWORK_ISOLATION_UNAVAILABLE:%.*s:%s\n",
			static_cast<int>(aStage.size()),
			aStage.data(),
			std::strerror(errno));
		::_exit(125);
	}

	// BPR-0890. The supervised child and the signal already passed on to it, in
	// the only object shape a signal handler may touch. They are deliberately
	// not a `pid_t` and not a lambda capture.
	volatile std::sig_atomic_t ChildPid        = 0;
	volatile std::sig_atomic_t ForwardedSignal = 0;

	// C linkage deliberately, and kept despite sitting in an unnamed namespace:
	// a signal handler is called from outside the program, and C linkage is what
	// the language guarantees for that.
	extern "C" void forward_to_child(const int aSignal)
	{
		ForwardedSignal  = aSignal;
		const auto child = ChildPid;
		if (child > 0) {
			// `kill` is async-signal-safe. `errno` is saved because this
			// handler interrupts a `waitpid` whose caller reads it.
			const auto saved = errno;
			::kill(static_cast<pid_t>(child), aSignal);
			errno = saved;
		}
	}

	[[nodiscard]] auto forward_signal(const int aSignal) -> bool
	{
		// `struct` is required: `sigaction` names the function in this scope.
		struct sigaction action{ };

		action.sa_handler = forward_to_child;
		if (::sigemptyset(&action.sa_mask) != 0) {
			return false;
		}
		// No `SA_RESTART`: the wait below loops on `EINTR` itself, so the
		// handler runs and the wait resumes without relying on the kernel to
		// restart it.
		action.sa_flags = 0;
		return ::sigaction(aSignal, &action, nullptr) == 0;
	}

	// The number `scripts/run_network_isolated_test.py` reports on macOS, so one
	// figure means the same thing on both platforms: `subprocess` yields a
	// negative value for death by a signal, and that runner prints it unchanged.
	// `_exit` keeps only the low eight bits, so passing the same negative number
	// on exit reproduces macOS's own status exactly -- a SIGABRT is printed as
	// -6 and exits 250 on both.
	[[nodiscard]] auto child_exit_code(const int aStatus) -> int
	{
		if (WIFSIGNALED(aStatus)) {
			return -WTERMSIG(aStatus);
		}
		if (WIFEXITED(aStatus)) {
			return WEXITSTATUS(aStatus);
		}
		// Unreachable: the wait asks about neither stopped nor continued
		// children, so no other disposition is reported.
		return 125;
	}

	void report_child_exit(const int aCode)
	{
		// The unconditional line is the evidence in every log; the second is
		// what `bpr_add_network_isolated_test` matches as its second
		// `FAIL_REGULAR_EXPRESSION` entry.
		std::printf("TEST_RUNNER_CHILD_EXIT:%d\n", aCode);
		if (aCode != 0) {
			std::printf("TEST_RUNNER_CHILD_EXIT_FAILED:%d\n", aCode);
		}
		std::fflush(stdout);
	}
} // namespace

auto main(const int argc, char** const argv) -> int
{
	if (argc < 2 || argv[1] == nullptr || argv[1][0] == '\0') {
		errno = EINVAL;
		fail("command");
	}
	const auto uid = ::getuid();
	const auto gid = ::getgid();
	if (::unshare(CLONE_NEWUSER | CLONE_NEWNET) != 0) {
		fail("unshare");
	}
	if (!map_current_identity(uid, gid)) {
		fail("identity-map");
	}
	if (!enable_loopback()) {
		fail("loopback");
	}
	// BPR-0890. Until this card the launcher `execvp`d here and *became* the
	// child, so it had no exit status left to state. CTest ignores the child's
	// status outright on any row carrying a `PASS_REGULAR_EXPRESSION`, so an
	// abort in the shutdown tail -- after `PUBLIC_PATH_OK` was already on
	// stdout -- left the row green. The launcher supervises instead of
	// dissolving, and states what the child returned.
	//
	// The boundary does not move. Namespaces are inherited across `fork`, so
	// the child runs in exactly the user and network namespaces created above,
	// and the identity mapping argued at the top of this file is written before
	// the fork and is untouched by it.
	const auto parent = ::getpid();
	// An inherited `SIG_IGN` for SIGCHLD survives `execve`, and under it the
	// child is reaped automatically, `waitpid` fails with `ECHILD`, and neither
	// exit line is printed -- leaving exactly the silent green this card exists
	// to remove. Nothing upstream sets it that way today, but the guard must not
	// depend on that.
	if (std::signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
		fail("sigchld-default");
	}
	// Nothing has been written to stdout yet, and nothing buffered may be
	// duplicated into both processes by the fork.
	std::fflush(stdout);
	const auto child = ::fork();
	if (child < 0) {
		fail("fork");
	}
	if (child == 0) {
		// A launcher killed outright -- which is what CTest's timeout does --
		// must not leave the test process running. The setting survives the
		// `execvp` below for every test binary in this tree: it is cleared only
		// for a set-uid, set-gid or file-capability image, or when the
		// credentials change, and no test binary is any of those.
		//
		// This covers the test process itself, not helper processes the test
		// goes on to spawn; a grandchild outlives the kill here exactly as it
		// did under the `execvp` launcher, so that is unchanged rather than
		// newly guaranteed.
		if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) {
			fail("pdeathsig");
		}
		// The parent may already have died in the window between the fork and
		// the call above, in which case that signal will never arrive.
		if (::getppid() != parent) {
			::_exit(125);
		}
		::execvp(argv[1], argv + 1);
		fail("exec");
	}
	ChildPid = child;
	// CTest's interruption now lands on this process rather than on the test,
	// so it is passed on. The macOS runner forwards the same three.
	if (!forward_signal(SIGINT) || !forward_signal(SIGTERM)
		|| !forward_signal(SIGHUP)) {
		fail("signal-handlers");
	}
	auto status = 0;
	while (::waitpid(child, &status, 0) < 0) {
		if (errno != EINTR) {
			fail("wait");
		}
	}
	// The pid is released by that reap, so a signal arriving during the printing
	// below must no longer be forwarded to it: the number could by then name a
	// different process. The macOS runner guards the same window by asking
	// `child.poll()` before it signals.
	ChildPid  = 0;
	auto code = child_exit_code(status);
	if (ForwardedSignal != 0 && code == 0) {
		// As the macOS runner does: a child that was signalled and still
		// reported success is not reported as a clean run.
		code = 128 + ForwardedSignal;
	}
	report_child_exit(code);
	::_exit(code);
}
