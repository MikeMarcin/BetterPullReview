/// \file
/// \brief Proves the authenticated launch channel refuses everything but the
///        bound supervisor.
///
/// BPR-0262 fact_4 requires the application to authenticate stage one before
/// `QApplication` and before public observation, and to refuse a direct or
/// alternate launch. [[BPR-0357]] proved the Mach mechanism exists under the
/// capsule's ad-hoc signature; [[BPR-0351]] moved it into the product. This
/// contract exercises the product's own halves against each other.
///
/// It links `designated_live_macos_provenance.cpp` rather than a copy, so the
/// code under test is the code that ships. It does not link the application or
/// the supervisor, because neither can be built in a tests-enabled graph — the
/// capsule option hard-fails against `BUILD_TESTING`. What is proven here is
/// the channel itself; that it runs before Qt is pinned separately, by the
/// architecture boundary checker reading `main.cpp`.

#include "designated_live_macos_provenance.hpp"

#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;

namespace {
	using bpr::app::designated_live_provenance::supervisor_channel;

	/// The child authenticates against the image it was launched from, which
	/// is what a real application does with its own capsule's supervisor.
	constexpr auto expect_self_argument
		= std::string_view{ "--provenance-child-expects-self" };
	/// The child authenticates against an image that is emphatically not its
	/// launcher, so its own identity check is the thing that has to refuse.
	constexpr auto expect_other_argument
		= std::string_view{ "--provenance-child-expects-other" };
	constexpr auto other_image = std::string_view{ "/usr/bin/true" };

	auto               failures = 0;
	std::vector<pid_t> pending;

	void reap_pending()
	{
		// A child spawned suspended and never resumed is a child nobody reaps,
		// and ctest would wait on it forever. BPR-0357 shipped that bug once
		// and had to repair it; this one starts repaired.
		for (const auto child : pending) {
			static_cast<void>(::kill(child, SIGKILL));
			auto status = 0;
			while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
			}
		}
		pending.clear();
	}

	void expect(const bool aCondition, const std::string_view aClaim)
	{
		if (!aCondition) {
			++failures;
			std::cerr << "FAIL: " << aClaim << '\n';
			reap_pending();
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto run_child(const std::filesystem::path& aExpected) -> int
	{
		return bpr::app::designated_live_provenance::authenticate_supervisor(
				   aExpected)
			? EXIT_SUCCESS
			: EXIT_FAILURE;
	}

	[[nodiscard]] auto spawn_suspended(char** aArgv, pid_t& aChild) -> bool
	{
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			return false;
		}
		static_cast<void>(posix_spawnattr_setflags(
			&attributes, static_cast<short>(POSIX_SPAWN_START_SUSPENDED)));
		const auto spawned = posix_spawn(
			&aChild, aArgv[0], nullptr, &attributes, aArgv, environ);
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		if (spawned == 0) {
			pending.push_back(aChild);
		}
		return spawned == 0;
	}

	[[nodiscard]] auto reap(const pid_t aChild) -> int
	{
		auto status = 0;
		while (::waitpid(aChild, &status, 0) < 0 && errno == EINTR) {
		}
		std::erase(pending, aChild);
		return WIFEXITED(status) != 0 ? WEXITSTATUS(status) : -1;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	const auto self = std::filesystem::path{ argv[0] };
	if (argc > 1) {
		const auto mode = std::string_view{ argv[1] };
		if (mode == expect_self_argument) {
			return run_child(self);
		}
		if (mode == expect_other_argument) {
			return run_child(std::filesystem::path{ other_image });
		}
	}

	auto  selfMode    = std::string{ expect_self_argument };
	auto  otherMode   = std::string{ expect_other_argument };
	char* selfArgv[]  = { argv[0], selfMode.data(), nullptr };
	char* otherArgv[] = { argv[0], otherMode.data(), nullptr };

	// The supported course, in the order the product performs it.
	{
		auto channel = supervisor_channel::claim();
		expect(
			channel.has_value(),
			"stage one claims an empty SERVICE_SLOT under ad-hoc signing");
		expect(
			!supervisor_channel::claim().has_value(),
			"a claim on an occupied slot refuses rather than overwriting it");

		pid_t child = 0;
		expect(
			spawn_suspended(selfArgv, child),
			"the application spawns suspended, inheriting the published right");
		expect(
			channel->restore(),
			"stage one restores its exact registered-port snapshot after the spawn");
		expect(::kill(child, SIGCONT) == 0, "the suspended application resumes");
		expect(
			channel->authenticate(child, self),
			"stage one authenticates the child from its audit trailer and answers");
		expect(
			reap(child) == EXIT_SUCCESS,
			"the application accepted the supervisor's exact reply and continued");
	}

	// The slot is free again, which is what the snapshot restore was for.
	{
		auto reclaimed = supervisor_channel::claim();
		expect(
			reclaimed.has_value(),
			"the restored snapshot leaves SERVICE_SLOT free for the next launch");
	}

	// Fail-closed, and the one fact_3 cares most about: a direct launch. No
	// right was published, so the application must refuse outright rather than
	// reach for argv, the environment, its parent's pid, or anything left on
	// disk. If it ever fell back, this course would succeed instead.
	{
		pid_t bare = 0;
		expect(
			spawn_suspended(selfArgv, bare),
			"a directly launched application spawns");
		expect(::kill(bare, SIGCONT) == 0, "the directly launched child resumes");
		expect(
			reap(bare) == EXIT_FAILURE,
			"an application that inherits no published right refuses outright");
	}

	// Fail-closed, the other direction of fact_4: the child is launched through
	// a real channel and receives the exact fixed reply, but the peer that sent
	// it is not the supervisor image the child expects. Answering with the
	// right word is not enough.
	{
		auto channel = supervisor_channel::claim();
		expect(channel.has_value(), "an alternate launcher can publish a right");
		pid_t child = 0;
		expect(
			spawn_suspended(otherArgv, child),
			"the application spawns under the alternate launcher");
		expect(channel->restore(), "the alternate launcher restores its snapshot");
		expect(::kill(child, SIGCONT) == 0, "the child resumes");
		expect(
			channel->authenticate(child, self),
			"the alternate launcher completes its side of the handshake");
		expect(
			reap(child) != EXIT_SUCCESS,
			"a reply from a peer whose code identity is not the expected "
			"supervisor is refused");
	}

	reap_pending();
	if (failures == 0) {
		std::cout << "macos-supervisor-provenance-tests:pass\n";
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
