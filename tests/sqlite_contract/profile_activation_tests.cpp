#include "bpr/adapters/sqlite/profile_activation.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>

#if !defined(_WIN32)
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/un.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr::adapters::sqlite;
	using outcome = bpr::application::profile_activation_outcome;
	using intent  = bpr::core::profile_launch_intent;
	constexpr bpr::core::bpr_profile_id profile{ 1U };
	void                                expect(bool aCondition, std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
#if !defined(_WIN32)
	class scratch_directory
	{
	public:
		scratch_directory()
		{
			// This class is POSIX-only, but the MSVC gate reads text rather than
			// branches, and the suppression is what the tree spells everywhere
			// `std::getenv` is read (BPR-0519, [[BPR-0854]]).
	#if defined(_MSC_VER)
		#pragma warning(push)
		#pragma warning(disable : 4996)
	#endif
			const auto* socketRoot = std::getenv("BPR_TEST_SOCKET_ROOT");
	#if defined(_MSC_VER)
		#pragma warning(pop)
	#endif
			auto pattern = (std::filesystem::path{ socketRoot ? socketRoot : "/tmp" } / "bpr-act-XXXXXX").string();
			expect(::mkdtemp(pattern.data()) != nullptr, "make private scratch");
			Path = std::filesystem::canonical(pattern.data());
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

	void test_authority()
	{
		const scratch_directory root;
		const scratch_directory other;
		auto                    lease = acquire_profile_runtime_lease(root.Path);
		expect(lease.has_value(), "acquire lease");
		expect(!make_profile_activation_server(root.Path, profile, { }), "null lease refused");
		expect(!make_profile_activation_server(root.Path, profile, std::make_shared<bpr::application::profile_runtime_lease>()), "forged lease refused");
		expect(!make_profile_activation_server(other.Path, profile, *lease), "wrong root lease refused");
		auto server = make_profile_activation_server(root.Path, profile, *lease);
		expect(server.has_value(), "bind admitted profile");
		expect(!make_profile_activation_server(root.Path, profile, *lease), "duplicate listener refused");
		lease->reset();
		expect(!acquire_profile_runtime_lease(root.Path), "server retains lease");
		server->reset();
		expect(!std::filesystem::exists(root.Path / "activate.sock"), "socket removed on drain");
		expect(acquire_profile_runtime_lease(root.Path).has_value(), "lease released after server");
		const auto missing = send_profile_activation(root.Path, profile, intent::activate);
		expect(!missing && missing.error() == outcome::unavailable, "missing owner unavailable");
		std::filesystem::create_symlink(other.Path, root.Path / "activate.sock");
		lease = acquire_profile_runtime_lease(root.Path);
		expect(!make_profile_activation_server(root.Path, profile, *lease), "socket symlink refused");
		expect(std::filesystem::is_symlink(root.Path / "activate.sock"), "refusal preserves hostile entry");
		expect(!send_profile_activation(root.Path, profile, intent::activate), "client refuses socket symlink");
	}

	auto launch_client(const std::filesystem::path& aRoot, const char* aIntent, const char* aExpected) -> pid_t
	{
		const auto child = ::fork();
		expect(child >= 0, "fork client");
		if (child == 0) {
			::execl(BPR_ACTIVATION_TEST_EXECUTABLE, BPR_ACTIVATION_TEST_EXECUTABLE, "--client", aRoot.c_str(), aIntent, aExpected, static_cast<char*>(nullptr));
			::_exit(127);
		}
		return child;
	}

	void await_child(pid_t aChild)
	{
		int status = 0;
		expect(::waitpid(aChild, &status, 0) == aChild && WIFEXITED(status) && WEXITSTATUS(status) == 0, "independent client terminal");
	}

	auto receive_request(const std::shared_ptr<bpr::application::profile_activation_server>& aServer) -> bpr::application::profile_activation_request
	{
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{ 3 };
		while (std::chrono::steady_clock::now() < until) {
			if (auto request = aServer->receive()) {
				return std::move(*request);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		expect(false, "bounded request arrival");
		std::abort();
	}

	void test_process_handoff()
	{
		const scratch_directory root;
		const scratch_directory other;
		auto                    lease      = acquire_profile_runtime_lease(root.Path);
		auto                    otherLease = acquire_profile_runtime_lease(other.Path);
		expect(lease && otherLease, "independent leases");
		auto server      = make_profile_activation_server(root.Path, profile, *lease);
		auto otherServer = make_profile_activation_server(other.Path, profile, *otherLease);
		expect(server && otherServer, "independent endpoints");
		const auto child   = launch_client(root.Path, "activate", "queued");
		auto       request = receive_request(*server);
		expect(request.Intent == intent::activate && !(*otherServer)->receive(), "exact profile route");
		expect(!(*server)->receive(), "one pending request bound");
		request.Reply->complete(outcome::queued);
		request.Reply->complete(outcome::refused);
		await_child(child);
		const auto second    = launch_client(other.Path, "new-window", "refused");
		auto       newWindow = receive_request(*otherServer);
		expect(newWindow.Intent == intent::new_window && !(*server)->receive(), "new-window exact intent");
		newWindow.Reply->complete(outcome::refused);
		await_child(second);
		const auto lost    = launch_client(root.Path, "new-window", "indeterminate");
		auto       pending = receive_request(*server);
		await_child(lost);
		// Client has timed out; acknowledgement must neither signal-kill nor admit a retry.
		pending.Reply->complete(outcome::queued);
		const auto closing      = launch_client(root.Path, "activate", "indeterminate");
		auto       closeRequest = receive_request(*server);
		server->reset();
		await_child(closing);
		closeRequest.Reply->complete(outcome::queued);
	}

	auto raw_client(const std::filesystem::path& aRoot) -> int
	{
		const int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
		expect(socket >= 0, "raw client socket");
		sockaddr_un address{ };
		address.sun_family = AF_UNIX;
		const auto path    = (aRoot / "activate.sock").string();
		expect(path.size() < sizeof(address.sun_path), "fixture socket path fits");
		std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
		expect(::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0, "raw connect");
		return socket;
	}

	void test_hostile_framing()
	{
		const scratch_directory root;
		auto                    lease  = acquire_profile_runtime_lease(root.Path);
		auto                    server = make_profile_activation_server(root.Path, profile, *lease);
		expect(server.has_value(), "framing server");
		const std::array<unsigned char, 17> valid{ 'B', 'P', 'R', 'A', 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
		for (unsigned int test = 0; test < 6U; ++test) {
			auto        bytes = valid;
			std::size_t size  = 16U;
			if (test == 0U) {
				bytes[4] = 2U;
			}
			if (test == 1U) {
				bytes[5] = 9U;
			}
			if (test == 2U) {
				bytes[8] = 2U;
			}
			if (test == 3U) {
				size = 17U;
			}
			if (test == 4U) {
				size = 15U;
			}
			if (test == 5U) {
				bytes[6] = 1U;
			}
			const auto client = raw_client(root.Path);
			expect(::send(client, bytes.data(), size, 0) == static_cast<ssize_t>(size), "hostile send");
			(void)::shutdown(client, SHUT_WR);
			for (int poll = 0; poll < 3; ++poll) {
				expect(!(*server)->receive(), "hostile frame never admitted");
			}
			::close(client);
		}
		const auto client = raw_client(root.Path);
		expect(::send(client, valid.data(), 8U, 0) == 8, "fragment one");
		expect(!(*server)->receive(), "partial frame waits");
		expect(::send(client, valid.data() + 8U, 8U, 0) == 8, "fragment two");
		expect(!(*server)->receive(), "frame requires EOF");
		(void)::shutdown(client, SHUT_WR);
		auto request = receive_request(*server);
		::close(client);
		request.Reply->complete(outcome::queued);
		const auto stalled = raw_client(root.Path);
		expect(!(*server)->receive(), "empty peer bounded");
		std::this_thread::sleep_for(std::chrono::milliseconds{ 2100 });
		expect(!(*server)->receive(), "stalled peer expires");
		::close(stalled);
		const auto child = launch_client(root.Path, "activate", "queued");
		receive_request(*server).Reply->complete(outcome::queued);
		await_child(child);
	}
#endif
}

auto main(int aCount, char** aArguments) -> int
{
#if !defined(_WIN32)
	if (aCount == 5 && std::string_view{ aArguments[1] } == "--client") {
		const auto             result = send_profile_activation(aArguments[2], profile, std::string_view{ aArguments[3] } == "activate" ? intent::activate : intent::new_window);
		const std::string_view expected{ aArguments[4] };
		return ((expected == "queued" && result && *result == outcome::queued) || (expected == "refused" && result && *result == outcome::refused) || (expected == "indeterminate" && !result && result.error() == outcome::indeterminate)) ? 0 : 1;
	}
	test_authority();
	test_process_handoff();
	test_hostile_framing();
#else
	(void)aCount;
	(void)aArguments;
	expect(!make_profile_activation_server({ }, profile, { }), "Windows explicit unavailable");
	expect(!send_profile_activation({ }, profile, intent::activate), "Windows client unavailable");
#endif
	std::cout << "profile-activation-tests:pass\n";
}
