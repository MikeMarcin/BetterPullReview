#include "bpr/adapters/sqlite/profile_activation.hpp"

#include "storage_internal.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <utility>

#if !defined(_WIN32)
	#include <cerrno>
	#include <fcntl.h>
	#include <poll.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <sys/un.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::sqlite {
	namespace {
		using outcome = application::profile_activation_outcome;
#if !defined(_WIN32)
		using clock_type                        = std::chrono::steady_clock;
		constexpr auto        deadline_interval = std::chrono::seconds{ 2 };
		constexpr std::size_t frame_size        = 16U;
		using frame                             = std::array<unsigned char, frame_size>;

		class socket_handle
		{
		public:
			explicit socket_handle(int aValue = -1)
				: Value(aValue)
			{ }
			~socket_handle()
			{
				if (Value >= 0) {
					::close(Value);
				}
			}
			socket_handle(const socket_handle&)                    = delete;
			auto operator=(const socket_handle&) -> socket_handle& = delete;
			socket_handle(socket_handle&& aOther) noexcept
				: Value(std::exchange(aOther.Value, -1))
			{ }
			auto operator=(socket_handle&& aOther) noexcept -> socket_handle&
			{
				if (this != &aOther) {
					socket_handle old{ std::exchange(Value, std::exchange(aOther.Value, -1)) };
				}
				return *this;
			}
			int Value;
		};

		auto configure_socket(int aSocket) -> bool
		{
	#if defined(__APPLE__)
			const int enabled = 1;
			if (::setsockopt(aSocket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
				return false;
			}
	#endif
			return aSocket >= 0 && ::fcntl(aSocket, F_SETFD, FD_CLOEXEC) == 0 && ::fcntl(aSocket, F_SETFL, O_NONBLOCK) == 0;
		}

		auto same_user(int aSocket) -> bool
		{
	#if defined(__APPLE__)
			uid_t user  = 0;
			gid_t group = 0;
			return ::getpeereid(aSocket, &user, &group) == 0 && user == ::geteuid();
	#elif defined(__linux__)
			ucred     credentials{ };
			socklen_t size = sizeof(credentials);
			return ::getsockopt(aSocket, SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0 && credentials.uid == ::geteuid();
	#else
			(void)aSocket;
			return false;
	#endif
		}

		auto endpoint(const std::filesystem::path& aRoot) -> std::optional<sockaddr_un>
		{
			if (!detail::validate_private_storage_root(aRoot)) {
				return std::nullopt;
			}
			const auto  path = (aRoot / "activate.sock").string();
			sockaddr_un address{ };
			if (path.size() >= sizeof(address.sun_path)) {
				return std::nullopt;
			}
			address.sun_family = AF_UNIX;
			std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
			return address;
		}

		auto private_socket(const char* aPath) -> bool
		{
			struct stat info{ };
			return ::lstat(aPath, &info) == 0 && S_ISSOCK(info.st_mode) && info.st_uid == ::geteuid() && (info.st_mode & 0777U) == 0600U;
		}

		auto make_frame(core::bpr_profile_id aProfile, core::profile_launch_intent aIntent) -> frame
		{
			frame result{ 'B', 'P', 'R', 'A', 1, static_cast<unsigned char>(aIntent), 0, 0 };
			for (std::size_t index = 0; index < 8U; ++index) {
				result[8U + index] = static_cast<unsigned char>((aProfile.value() >> (index * 8U)) & 255U);
			}
			return result;
		}

		auto send_bytes(int aSocket, const void* aData, std::size_t aSize) -> ssize_t
		{
	#if defined(MSG_NOSIGNAL)
			return ::send(aSocket, aData, aSize, MSG_NOSIGNAL);
	#else
			return ::send(aSocket, aData, aSize, 0);
	#endif
		}

		class native_reply final : public application::profile_activation_reply
		{
		public:
			native_reply(socket_handle aSocket, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Socket(std::move(aSocket))
				, Lease(std::move(aLease))
			{ }
			~native_reply() override { complete(outcome::indeterminate); }
			void complete(outcome aOutcome) noexcept override
			{
				const std::lock_guard lock{ Mutex };
				if (Socket.Value < 0) {
					return;
				}
				const std::array<unsigned char, 2> response{ 1U, aOutcome == outcome::queued ? static_cast<unsigned char>(1U) : static_cast<unsigned char>(2U) };
				if (aOutcome == outcome::queued || aOutcome == outcome::refused) {
					(void)send_bytes(Socket.Value, response.data(), response.size());
				}
				Socket = socket_handle{ };
				Lease.reset();
			}
			auto pending() -> bool
			{
				const std::lock_guard lock{ Mutex };
				return Socket.Value >= 0;
			}

		private:
			std::mutex                                          Mutex;
			socket_handle                                       Socket;
			std::shared_ptr<application::profile_runtime_lease> Lease;
		};

		class native_server final : public application::profile_activation_server
		{
		public:
			native_server(socket_handle aListener, std::filesystem::path aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Listener(std::move(aListener))
				, Root(std::move(aRoot))
				, Profile(aProfile)
				, Lease(std::move(aLease))
			{
				(void)::lstat((Root / "activate.sock").c_str(), &Identity);
			}
			~native_server() override
			{
				if (auto reply = Pending.lock()) {
					reply->complete(outcome::indeterminate);
				}
				Listener = socket_handle{ };
				Client   = socket_handle{ };
				try {
					struct stat named{ };
					const auto  path = Root / "activate.sock";
					if (detail::validates_profile_runtime_lease(Root, Lease) && ::lstat(path.c_str(), &named) == 0 && named.st_dev == Identity.st_dev && named.st_ino == Identity.st_ino) {
						(void)::unlink(path.c_str());
					}
				} catch (...) {
					// A later admitted owner can remove a stale socket; destruction must still drain descriptors.
				}
			}
			auto receive() -> std::optional<application::profile_activation_request> override
			{
				if (auto reply = Pending.lock(); reply && reply->pending()) {
					return std::nullopt;
				}
				if (Client.Value < 0) {
					Client = socket_handle{ ::accept(Listener.Value, nullptr, nullptr) };
					if (Client.Value < 0) {
						return std::nullopt;
					}
					if (!configure_socket(Client.Value) || !same_user(Client.Value)) {
						Client = socket_handle{ };
						return std::nullopt;
					}
					Deadline = clock_type::now() + deadline_interval;
					Used     = 0U;
				}
				if (clock_type::now() >= Deadline) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				// One bounded read per poll, including one extra byte to refuse overlong framing.
				// The descriptor is O_NONBLOCK; weak_ptr::lock above is not a held mutex.
				const auto count = ::recv(Client.Value, Input.data() + Used, Input.size() - Used, 0); // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
				if (count < 0) {
					if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
						Client = socket_handle{ };
					}
					return std::nullopt;
				}
				if (count > 0) {
					Used += static_cast<std::size_t>(count);
					if (Used > frame_size) {
						Client = socket_handle{ };
					}
					return std::nullopt;
				}
				// A write-half close commits exactly one frame; fragmented and delayed trailing data cannot be mistaken for a complete request.
				if (Used != frame_size) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				const auto intent   = static_cast<core::profile_launch_intent>(Input[5]);
				const auto expected = make_frame(Profile, intent);
				if ((intent != core::profile_launch_intent::activate && intent != core::profile_launch_intent::new_window) || !std::equal(expected.begin(), expected.end(), Input.begin())) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				auto reply = std::make_shared<native_reply>(std::move(Client), Lease);
				Pending    = reply;
				return application::profile_activation_request{ intent, std::move(reply) };
			}

		private:
			socket_handle                                       Listener;
			std::filesystem::path                               Root;
			core::bpr_profile_id                                Profile;
			std::shared_ptr<application::profile_runtime_lease> Lease;
			struct stat                                         Identity{ };
			socket_handle                                       Client;
			std::array<unsigned char, frame_size + 1U>          Input{ };
			std::size_t                                         Used = 0U;
			clock_type::time_point                              Deadline;
			std::weak_ptr<native_reply>                         Pending;
		};

		auto wait_socket(int aSocket, short aEvents, clock_type::time_point aDeadline, std::stop_token aStop) -> bool
		{
			while (!aStop.stop_requested() && clock_type::now() < aDeadline) {
				pollfd     descriptor{ aSocket, aEvents, 0 };
				const auto result = ::poll(&descriptor, 1, 20);
				if (result > 0) {
					return (static_cast<unsigned short>(descriptor.revents) & static_cast<unsigned short>(aEvents)) != 0U;
				}
				if (result < 0 && errno != EINTR) {
					return false;
				}
			}
			return false;
		}
#endif
	}

	auto make_profile_activation_server(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
		-> std::expected<std::shared_ptr<application::profile_activation_server>, outcome>
	{
#if defined(_WIN32)
		(void)aRoot;
		(void)aProfile;
		(void)aLease;
		return std::unexpected(outcome::unavailable);
#else
		try {
			if (aProfile.value() == 0U || !detail::validates_profile_runtime_lease(aRoot, aLease)) {
				return std::unexpected(outcome::unavailable);
			}
			const auto address = endpoint(aRoot);
			if (!address) {
				return std::unexpected(outcome::unavailable);
			}
			struct stat existing{ };
			if (::lstat(address->sun_path, &existing) == 0) {
				if (!private_socket(address->sun_path)) {
					return std::unexpected(outcome::unavailable);
				}
				socket_handle probe{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
				if (!configure_socket(probe.Value) || ::connect(probe.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) == 0 || errno != ECONNREFUSED) {
					return std::unexpected(outcome::unavailable);
				}
				if (::unlink(address->sun_path) != 0) {
					return std::unexpected(outcome::unavailable);
				}
			} else if (errno != ENOENT) {
				return std::unexpected(outcome::unavailable);
			}
			socket_handle listener{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
			if (!configure_socket(listener.Value) || ::bind(listener.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) != 0) {
				return std::unexpected(outcome::unavailable);
			}
			// The parent is already 0700, so no other user can reach the socket before chmod.
			if (::chmod(address->sun_path, 0600) != 0 || ::listen(listener.Value, 8) != 0) {
				(void)::unlink(address->sun_path);
				return std::unexpected(outcome::unavailable);
			}
			return std::make_shared<native_server>(std::move(listener), aRoot, aProfile, std::move(aLease));
		} catch (...) {
			return std::unexpected(outcome::unavailable);
		}
#endif
	}

	auto send_profile_activation(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, core::profile_launch_intent aIntent, std::stop_token aStop)
		-> std::expected<outcome, outcome>
	{
#if defined(_WIN32)
		(void)aRoot;
		(void)aProfile;
		(void)aIntent;
		(void)aStop;
		return std::unexpected(outcome::unavailable);
#else
		bool sent = false;
		try {
			if (aStop.stop_requested() || aProfile.value() == 0U || (aIntent != core::profile_launch_intent::activate && aIntent != core::profile_launch_intent::new_window)) {
				return std::unexpected(outcome::unavailable);
			}
			const auto address = endpoint(aRoot);
			if (!address || !private_socket(address->sun_path)) {
				return std::unexpected(outcome::unavailable);
			}
			socket_handle socket{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
			if (!configure_socket(socket.Value)) {
				return std::unexpected(outcome::unavailable);
			}
			const auto deadline = clock_type::now() + deadline_interval;
			if (::connect(socket.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) != 0) {
				if (errno != EINPROGRESS || !wait_socket(socket.Value, POLLOUT, deadline, aStop)) {
					return std::unexpected(outcome::unavailable);
				}
				int       error = 0;
				socklen_t size  = sizeof(error);
				if (::getsockopt(socket.Value, SOL_SOCKET, SO_ERROR, &error, &size) != 0 || error != 0) {
					return std::unexpected(outcome::unavailable);
				}
			}
			if (!same_user(socket.Value) || aStop.stop_requested()) {
				return std::unexpected(outcome::unavailable);
			}
			const auto request = make_frame(aProfile, aIntent);
			const auto count   = send_bytes(socket.Value, request.data(), request.size());
			sent               = count > 0;
			if (count != static_cast<ssize_t>(request.size()) || ::shutdown(socket.Value, SHUT_WR) != 0) {
				return std::unexpected(sent ? outcome::indeterminate : outcome::unavailable);
			}
			std::array<unsigned char, 2> response{ };
			std::size_t                  used = 0U;
			while (used < response.size() && wait_socket(socket.Value, POLLIN, deadline, aStop)) {
				const auto received = ::recv(socket.Value, response.data() + used, response.size() - used, 0);
				if (received <= 0) {
					break;
				}
				used += static_cast<std::size_t>(received);
			}
			if (used != response.size() || response[0] != 1U || (response[1] != 1U && response[1] != 2U)) {
				return std::unexpected(outcome::indeterminate);
			}
			return response[1] == 1U ? outcome::queued : outcome::refused;
		} catch (...) {
			return std::unexpected(sent ? outcome::indeterminate : outcome::unavailable);
		}
#endif
	}
}
