// Bundled Unix process owner. Descriptor 3 is the application's lifetime/start
// channel; optional descriptor 4 retains the exact workspace lifetime lock.
// Standard streams belong to the selected agent protocol. No Qt,
// repository, credential, settings or network capability is linked here.
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <string_view>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
	constexpr int Owner   = 3;
	constexpr int Failure = 125;

	void close_descriptor(int aDescriptor) noexcept
	{
		if (aDescriptor >= 0) {
			static_cast<void>(::close(aDescriptor));
		}
	}

	[[nodiscard]] auto report_start(int aError) noexcept -> bool
	{
		const auto value   = static_cast<std::int32_t>(aError);
		auto       written = ssize_t{ };
		do {
			written = ::write(Owner, &value, sizeof(value));
		} while (written < 0 && errno == EINTR);
		return written == sizeof(value);
	}

	[[nodiscard]] auto finish_child(pid_t aChild) noexcept -> int
	{
		// The direct child is never reaped before this group signal. Its PID
		// therefore cannot be reused for an unrelated process group.
		static_cast<void>(::kill(-aChild, SIGKILL));
		int   status = 0;
		pid_t waited;
		do {
			waited = ::waitpid(aChild, &status, 0);
		} while (waited < 0 && errno == EINTR);
		if (waited != aChild) {
			return Failure;
		}
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		}
		if (WIFSIGNALED(status)) {
			const auto signal = WTERMSIG(status);
			static_cast<void>(::signal(signal, SIG_DFL));
			static_cast<void>(::raise(signal));
		}
		return Failure;
	}
}

auto main(int argc, char** argv) -> int
{
	const auto workspaceLease = argc > 1 && std::string_view{ argv[1] } == "--workspace-lease";
	if (workspaceLease) {
		--argc;
		++argv;
		struct stat lease{ };
		if (::fstat(4, &lease) != 0 || !S_ISREG(lease.st_mode)
			|| (::fcntl(4, F_GETFL) & O_ACCMODE) != O_RDONLY
			|| ::fcntl(4, F_SETFD, FD_CLOEXEC) != 0) {
			return Failure;
		}
	}

	int       socketType      = 0;
	socklen_t socketTypeBytes = sizeof(socketType);
	if (argc < 2 || argv[1][0] != '/'
		|| ::getsockopt(Owner, SOL_SOCKET, SO_TYPE, &socketType, &socketTypeBytes) != 0
		|| socketType != SOCK_STREAM || ::fcntl(Owner, F_SETFD, FD_CLOEXEC) != 0) {
		return Failure;
	}
	static_cast<void>(::signal(SIGPIPE, SIG_IGN));
	int  execution[2] = { -1, -1 };
	int  group[2]     = { -1, -1 };
	auto closePipes   = [&execution, &group]() noexcept {
		for (const auto descriptor : { execution[0], execution[1], group[0], group[1] }) {
			close_descriptor(descriptor);
		}
	};
	if (::pipe(execution) != 0 || ::pipe(group) != 0
		|| ::fcntl(execution[0], F_SETFD, FD_CLOEXEC) != 0
		|| ::fcntl(execution[1], F_SETFD, FD_CLOEXEC) != 0
		|| ::fcntl(group[0], F_SETFD, FD_CLOEXEC) != 0
		|| ::fcntl(group[1], F_SETFD, FD_CLOEXEC) != 0) {
		static_cast<void>(report_start(errno));
		closePipes();
		return Failure;
	}
	const auto child = ::fork();
	if (child < 0) {
		static_cast<void>(report_start(errno));
		closePipes();
		return Failure;
	}
	if (child == 0) {
		close_descriptor(Owner);
		close_descriptor(execution[0]);
		close_descriptor(group[1]);
		if (workspaceLease) {
			close_descriptor(4);
		}
		static_cast<void>(::signal(SIGPIPE, SIG_DFL));
		// The parent alone creates this process group; the child waits for it
		// and never calls setpgid. When both sides raced to create the group,
		// Darwin sometimes refused the child's setpgid(0, 0) with EPERM, and
		// the launch ended as failed_to_start (BPR-0809). End of file means
		// the parent gave up, so the agent never runs outside its group.
		auto released = char{ 0 };
		auto count    = ssize_t{ };
		do {
			count = ::read(group[0], &released, 1U);
		} while (count < 0 && errno == EINTR);
		if (count == 1 && ::getpgrp() == ::getpid()) {
			::execv(argv[1], argv + 1);
		} else if (count >= 0) {
			errno = EPERM;
		}
		const auto error = static_cast<std::int32_t>(errno);
		// glibc declares write warn_unused_result under _FORTIFY_SOURCE, and GCC
		// does not accept a void cast of the call as using it, so the Linux
		// Release graph failed here. A lost report still reaches the owner as
		// end-of-file followed by this child's exit.
		const auto written = ::write(execution[1], &error, sizeof(error));
		static_cast<void>(written);
		std::_Exit(127);
	}
	close_descriptor(execution[1]);
	close_descriptor(group[0]);
	// The group exists before the child can exec, so the group signal in
	// finish_child also covers owner death before the agent starts. The child
	// cannot have exec'd yet, so every setpgid failure here is real.
	auto established = ::setpgid(child, child) == 0;
	if (established) {
		constexpr auto release = char{ 1 };
		auto           written = ssize_t{ };
		do {
			written = ::write(group[1], &release, 1U);
		} while (written < 0 && errno == EINTR);
		established = written == 1;
	}
	const auto groupError = errno;
	close_descriptor(group[1]);
	if (!established) {
		static_cast<void>(report_start(groupError != 0 ? groupError : EIO));
		static_cast<void>(::kill(child, SIGKILL));
		close_descriptor(execution[0]);
		return finish_child(child);
	}
	bool started = false;
	for (;;) {
		pollfd     descriptors[2] = { { Owner, POLLIN, 0 }, { execution[0], POLLIN, 0 } };
		const auto polled         = ::poll(descriptors, started ? 1U : 2U, 10);
		if (polled < 0 && errno == EINTR) {
			continue;
		}
		if (polled < 0 || descriptors[0].revents != 0) {
			close_descriptor(execution[0]);
			return finish_child(child);
		}
		if (!started && descriptors[1].revents != 0) {
			std::int32_t error = 0;
			const auto   count = ::read(execution[0], &error, sizeof(error));
			if (count < 0 && errno == EINTR) {
				continue;
			}
			const auto launchError = count == 0 ? 0 : count == sizeof(error) ? error
																			 : EIO;
			if (!report_start(launchError) || launchError != 0) {
				close_descriptor(execution[0]);
				return finish_child(child);
			}
			close_descriptor(execution[0]);
			execution[0] = -1;
			started      = true;
		}
		if (started) {
			siginfo_t state{ };
			if (::waitid(P_PID, static_cast<id_t>(child), &state, WEXITED | WNOHANG | WNOWAIT) != 0) {
				if (errno == EINTR) {
					continue;
				}
				return finish_child(child);
			}
			// Darwin can report CLD_STOPPED even with WEXITED. A stopped
			// writer is still alive and must retain its lifetime exclusion.
			if (state.si_pid == child
				&& (state.si_code == CLD_EXITED || state.si_code == CLD_KILLED || state.si_code == CLD_DUMPED)) {
				return finish_child(child);
			}
		}
	}
}
