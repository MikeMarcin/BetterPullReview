#if !defined(__linux__)
	#error "The network namespace launcher is Linux-only"
#endif

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <sched.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
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

	[[nodiscard]] auto map_current_identity(
		const uid_t aUid,
		const gid_t aGid) -> bool
	{
		const auto uidMap = std::string{ "0 " } + std::to_string(aUid) + " 1\n";
		const auto gidMap = std::string{ "0 " } + std::to_string(aGid) + " 1\n";
		if (!write_text("/proc/self/setgroups", "deny\n") && errno != ENOENT) {
			return false;
		}
		return write_text("/proc/self/uid_map", uidMap)
			&& write_text("/proc/self/gid_map", gidMap)
			&& ::setresgid(0, 0, 0) == 0
			&& ::setresuid(0, 0, 0) == 0;
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
	::execvp(argv[1], argv + 1);
	fail("exec");
}
