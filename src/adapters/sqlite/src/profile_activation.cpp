#include "bpr/adapters/sqlite/profile_activation.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include <random>

#include "storage_internal.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#include "../../native/windows_private_storage.hpp"
#else
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
		using outcome                           = application::profile_activation_outcome;
		using clock_type                        = std::chrono::steady_clock;
		constexpr auto        deadline_interval = std::chrono::seconds{ 2 };
		constexpr std::size_t frame_size        = 16U;
		using frame                             = std::array<unsigned char, frame_size>;

		/// \brief The one request both launches agree on: magic, version, intent, profile.
		auto make_frame(core::bpr_profile_id aProfile, core::profile_launch_intent aIntent) -> frame
		{
			frame result{ 'B', 'P', 'R', 'A', 1, static_cast<unsigned char>(aIntent), 0, 0 };
			for (std::size_t index = 0; index < 8U; ++index) {
				result[8U + index] = static_cast<unsigned char>((aProfile.value() >> (index * 8U)) & 255U);
			}
			return result;
		}

		/// \brief Admits exactly one complete frame for this profile, or nothing.
		auto frame_intent(const frame& aInput, core::bpr_profile_id aProfile) -> std::optional<core::profile_launch_intent>
		{
			const auto intent   = static_cast<core::profile_launch_intent>(aInput[5]);
			const auto expected = make_frame(aProfile, intent);
			if ((intent != core::profile_launch_intent::activate && intent != core::profile_launch_intent::new_window) || !std::equal(expected.begin(), expected.end(), aInput.begin())) {
				return std::nullopt;
			}
			return intent;
		}

		/// \brief Names the endpoint by a digest of the canonical root, so both launches agree.
		auto endpoint_digest(std::span<const std::uint8_t> aNative) -> std::string
		{
			const auto                 digest = core::sha256(aNative);
			constexpr std::string_view hex{ "0123456789abcdef" };
			std::string                result;
			for (std::size_t index = 0; index < 8U; ++index) {
				const auto byte = static_cast<unsigned>(digest[index]);
				result.push_back(hex[byte >> 4U]);
				result.push_back(hex[byte & 15U]);
			}
			return result;
		}
#if !defined(_WIN32)

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

		auto owner_private_directory(const std::filesystem::path& aPath, const bool aFollowLinks) -> bool
		{
			struct stat info{ };
			const auto  queried = aFollowLinks ? ::stat(aPath.c_str(), &info) : ::lstat(aPath.c_str(), &info);
			return queried == 0 && S_ISDIR(info.st_mode) && info.st_uid == ::geteuid() && (info.st_mode & 0777U) == 0700U;
		}

		auto environment_directory(const char* aName) -> std::optional<std::filesystem::path>
		{
			// The MSVC gate reads text rather than branches, and the suppression is what the
			// tree spells everywhere `std::getenv` is read (BPR-0519, [[BPR-0854]]).
	#if defined(_MSC_VER)
		#pragma warning(push)
		#pragma warning(disable : 4996)
	#endif
			const auto* configured = std::getenv(aName);
	#if defined(_MSC_VER)
		#pragma warning(pop)
	#endif
			if (configured == nullptr || configured[0] != '/') {
				return std::nullopt;
			}
			return std::filesystem::path{ configured };
		}

		auto endpoint_name(const std::filesystem::path& aRoot) -> std::string
		{
			const auto native = aRoot.lexically_normal().string();
			return "bpr-activate-" + endpoint_digest({ reinterpret_cast<const std::uint8_t*>(native.data()), native.size() }) + ".sock";
		}

		/// \brief Whether the endpoint's path in this directory, with its terminator, fits `sun_path`.
		auto endpoint_fits(const std::filesystem::path& aDirectory, const std::string& aName) -> bool
		{
			return (aDirectory / aName).string().size() < sizeof(sockaddr_un::sun_path);
		}

		/// \brief Finds the short per-user runtime directory the rendezvous belongs in.
		/// \details The candidates are the per-user runtime directories the platforms already
		/// keep owner-private, which is why the endpoint can leave the data root without losing
		/// an ownership guarantee: Linux's `$XDG_RUNTIME_DIR`, then `/run/user/<uid>`, then
		/// `$TMPDIR`, which is what macOS gives each user. BPR-0894 made the variable readable
		/// here: the transport owns *constructing* a child's environment, not the act of reading
		/// a variable to find a directory. [[BPR-0906]] adopted it, so that whoever starts the
		/// process decides where its endpoint lives. A Linux test launcher gives each row its
		/// own directory, and a row that kills its owner leaves nothing behind in the user's.
		/// The variable is read on Linux only: macOS gives the same role to `$TMPDIR`, and its
		/// test runner confines sockets to the directory it names there. The variable counts
		/// only when it names an absolute directory that is itself owner-private, and it earns
		/// no child. So a value inherited through `sudo` cannot put the endpoint inside another
		/// user's directory. Nor does a directory too long for the endpoint to fit `sun_path`
		/// count ([[BPR-1113]]): it is passed over, and the next candidate holds the endpoint,
		/// as it did before the variable counted. `/run/user/<uid>` is still probed directly,
		/// which covers a system that sets no such variable.
		/// Each candidate is verified owner-private rather than assumed. The variable and the
		/// top-level candidates are checked through a symbolic link, as the directory it leads
		/// to. Only the one owner-private child that a shared candidate such as `/tmp` earns is
		/// verified as a real directory, so a symlink planted in its place is refused rather
		/// than followed ([[BPR-1113]]). No existing permissions are widened.
		///
		/// On macOS the candidate that verifies is `$TMPDIR`, and `dirhelper` age-reaps it daily.
		/// BPR-0895 measured the reap: it removes a regular file whose birth and access times are
		/// both more than three days old and which no process holds open, and it skips sockets.
		/// An owner idle for longer therefore keeps its endpoint's name, but only while the socket
		/// is all the rendezvous keeps in this directory; `sqlite_profile_activation` holds that.
		/// A reboot empties the directory, and it ends the owner as well.
		auto runtime_directory([[maybe_unused]] const std::string& aEndpointName) -> std::optional<std::filesystem::path>
		{
			const auto identity = std::to_string(::geteuid());
			const auto owned    = "bpr-" + identity;
	#if defined(__linux__)
			if (const auto configured = environment_directory("XDG_RUNTIME_DIR"); configured && endpoint_fits(*configured, aEndpointName) && owner_private_directory(*configured, true)) {
				return configured;
			}
	#endif
			for (const auto& candidate : { std::optional<std::filesystem::path>{ std::filesystem::path{ "/run/user" } / identity }, environment_directory("TMPDIR"), std::optional<std::filesystem::path>{ "/tmp" } }) {
				if (!candidate) {
					continue;
				}
				if (owner_private_directory(*candidate, true)) {
					return candidate;
				}
				auto child = *candidate / owned;
				if ((::mkdir(child.c_str(), 0700) == 0 || errno == EEXIST) && owner_private_directory(child, false)) {
					return child;
				}
			}
			return std::nullopt;
		}

		auto resolve_endpoint(const std::filesystem::path& aRoot)
			-> std::expected<std::filesystem::path, profile_activation_endpoint_failure>
		{
			// The root still gates the rendezvous: a caller cannot meet on a root it does not own.
			if (!detail::validate_private_storage_root(aRoot)) {
				return std::unexpected(profile_activation_endpoint_failure::root_refused);
			}
			const auto name      = endpoint_name(aRoot);
			const auto directory = runtime_directory(name);
			if (!directory) {
				return std::unexpected(profile_activation_endpoint_failure::runtime_directory_absent);
			}
			if (!endpoint_fits(*directory, name)) {
				return std::unexpected(profile_activation_endpoint_failure::path_too_long);
			}
			return *directory / name;
		}

		auto endpoint_address(const std::filesystem::path& aPath) -> std::optional<sockaddr_un>
		{
			const auto  native = aPath.string();
			sockaddr_un address{ };
			if (native.size() >= sizeof(address.sun_path)) {
				return std::nullopt;
			}
			address.sun_family = AF_UNIX;
			std::memcpy(address.sun_path, native.c_str(), native.size() + 1U);
			return address;
		}

		auto private_socket(const char* aPath) -> bool
		{
			struct stat info{ };
			return ::lstat(aPath, &info) == 0 && S_ISSOCK(info.st_mode) && info.st_uid == ::geteuid() && (info.st_mode & 0777U) == 0600U;
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
			native_server(socket_handle aListener, std::filesystem::path aRoot, std::filesystem::path aEndpoint, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Listener(std::move(aListener))
				, Root(std::move(aRoot))
				, Endpoint(std::move(aEndpoint))
				, Profile(aProfile)
				, Lease(std::move(aLease))
			{
				(void)::lstat(Endpoint.c_str(), &Identity);
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
					// Still the root's own lease that admits removal, and still exactly the entry
					// this owner bound: the rendezvous moved, the ownership proof did not.
					if (detail::validates_profile_runtime_lease(Root, Lease) && ::lstat(Endpoint.c_str(), &named) == 0 && named.st_dev == Identity.st_dev && named.st_ino == Identity.st_ino) {
						(void)::unlink(Endpoint.c_str());
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
				frame received{ };
				std::copy_n(Input.begin(), frame_size, received.begin());
				const auto intent = frame_intent(received, Profile);
				if (!intent) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				auto reply = std::make_shared<native_reply>(std::move(Client), Lease);
				Pending    = reply;
				return application::profile_activation_request{ *intent, std::move(reply) };
			}

		private:
			socket_handle                                       Listener;
			std::filesystem::path                               Root;
			std::filesystem::path                               Endpoint;
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
#else
		// [[BPR-0979]]. The Windows rendezvous is an owner-private named pipe. It has
		// no filesystem entry to leave behind, so there is no stale-socket recovery and
		// nothing to unlink; the name is derived from the canonical root exactly as
		// the POSIX endpoint is. One 16-byte message is the whole request, so message
		// mode lets the pipe itself delimit the frame: a shorter, longer or split
		// message is refused rather than reassembled.
		using native_storage::private_security;
		using native_storage::private_security_attributes;
		using native_storage::validate_private_security;
		using native_storage::windows_handle;
		using native_storage::windows_object_kind;
		// One instance travels with a delivered request while a second keeps the name
		// bound, so the name is never free for another process to take while an owner lives.
		constexpr DWORD pipe_instances = 2U;
		constexpr DWORD pipe_buffer    = 64U;

		auto pipe_name(const std::wstring& aCanonicalRoot) -> std::wstring
		{
			const auto digest = endpoint_digest({ reinterpret_cast<const std::uint8_t*>(aCanonicalRoot.data()), aCanonicalRoot.size() * sizeof(wchar_t) });
			return L"\\\\.\\pipe\\bpr-activate-" + std::wstring{ digest.begin(), digest.end() };
		}

		auto resolve_endpoint(const std::filesystem::path& aRoot)
			-> std::expected<std::filesystem::path, profile_activation_endpoint_failure>
		{
			// The root still gates the rendezvous: a caller cannot meet on a root it does not own.
			if (!detail::validate_private_storage_root(aRoot)) {
				return std::unexpected(profile_activation_endpoint_failure::root_refused);
			}
			const auto canonical = native_storage::canonical_windows_root(aRoot);
			if (!canonical) {
				return std::unexpected(profile_activation_endpoint_failure::unavailable);
			}
			return std::filesystem::path{ pipe_name(canonical->native()) };
		}

		/// \brief Whether the process at the other end of the pipe runs as this user.
		auto same_user(HANDLE aPipe, const bool aServerSide, const private_security& aSecurity) -> bool
		{
			DWORD process = 0U;
			if ((aServerSide ? ::GetNamedPipeClientProcessId(aPipe, &process) : ::GetNamedPipeServerProcessId(aPipe, &process)) == 0 || process == 0U) {
				return false;
			}
			const windows_handle peer{ ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process) };
			if (!peer.valid()) {
				return false;
			}
			windows_handle token;
			if (::OpenProcessToken(peer.Value, TOKEN_QUERY, &token.Value) == 0) {
				return false;
			}
			DWORD required = 0U;
			(void)::GetTokenInformation(token.Value, TokenUser, nullptr, 0U, &required);
			if (required == 0U || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				return false;
			}
			auto information = std::vector<std::byte>(required);
			if (::GetTokenInformation(token.Value, TokenUser, information.data(), required, &required) == 0) {
				return false;
			}
			const auto* user = reinterpret_cast<const TOKEN_USER*>(information.data());
			return user->User.Sid != nullptr && ::IsValidSid(user->User.Sid) != 0 && ::EqualSid(user->User.Sid, aSecurity.sid()) != 0;
		}

		/// \brief One overlapped operation, polled without blocking the effect lane.
		class pipe_operation
		{
		public:
			enum class state : std::uint8_t { pending,
											  complete,
											  failed };
			pipe_operation()
				: Event{ ::CreateEventW(nullptr, TRUE, FALSE, nullptr) }
			{
				Overlapped.hEvent = Event.Value;
			}
			pipe_operation(const pipe_operation&)                    = delete;
			auto operator=(const pipe_operation&) -> pipe_operation& = delete;
			~pipe_operation()                                        = default;
			[[nodiscard]] auto valid() const noexcept -> bool { return Event.valid(); }
			[[nodiscard]] auto overlapped() noexcept -> OVERLAPPED* { return &Overlapped; }
			void               reset() noexcept
			{
				Overlapped        = OVERLAPPED{ };
				Overlapped.hEvent = Event.Value;
				(void)::ResetEvent(Event.Value);
			}
			/// \brief Polls once; `aBytes` and `aError` describe a finished operation.
			auto poll(HANDLE aPipe, DWORD& aBytes, DWORD& aError) -> state
			{
				aBytes = 0U;
				if (::GetOverlappedResult(aPipe, &Overlapped, &aBytes, FALSE) != 0) {
					aError = ERROR_SUCCESS;
					return state::complete;
				}
				aError = ::GetLastError();
				return aError == ERROR_IO_INCOMPLETE || aError == ERROR_IO_PENDING ? state::pending : state::failed;
			}
			/// \brief Waits until the deadline for a client-side step; cancels what did not finish.
			auto await(HANDLE aPipe, clock_type::time_point aDeadline, std::stop_token aStop, DWORD& aBytes, DWORD& aError) -> bool
			{
				while (!aStop.stop_requested() && clock_type::now() < aDeadline) {
					const auto result = poll(aPipe, aBytes, aError);
					if (result == state::complete) {
						return true;
					}
					if (result == state::failed) {
						return false;
					}
					const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(aDeadline - clock_type::now()).count();
					(void)::WaitForSingleObject(Event.Value, static_cast<DWORD>(std::clamp<long long>(remaining, 1, 20)));
				}
				(void)::CancelIoEx(aPipe, &Overlapped);
				DWORD ignored = 0U;
				(void)::GetOverlappedResult(aPipe, &Overlapped, &ignored, TRUE);
				aError = ERROR_TIMEOUT;
				return false;
			}

		private:
			windows_handle Event;
			OVERLAPPED     Overlapped{ };
		};

		class native_reply final : public application::profile_activation_reply
		{
		public:
			native_reply(windows_handle aPipe, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Pipe(std::move(aPipe))
				, Lease(std::move(aLease))
			{ }
			~native_reply() override { complete(outcome::indeterminate); }
			void complete(outcome aOutcome) noexcept override
			{
				const std::lock_guard lock{ Mutex };
				if (!Pipe.valid()) {
					return;
				}
				if (aOutcome == outcome::queued || aOutcome == outcome::refused) {
					const std::array<unsigned char, 2> response{ 1U, aOutcome == outcome::queued ? static_cast<unsigned char>(1U) : static_cast<unsigned char>(2U) };
					pipe_operation                     write;
					if (write.valid() && (::WriteFile(Pipe.Value, response.data(), static_cast<DWORD>(response.size()), nullptr, write.overlapped()) != 0 || ::GetLastError() == ERROR_IO_PENDING)) {
						DWORD bytes = 0U;
						DWORD error = ERROR_SUCCESS;
						// Bounded: a peer that never drains its own reply cannot hold the effect lane.
						(void)write.await(Pipe.Value, clock_type::now() + deadline_interval, { }, bytes, error);
					}
				}
				// Closing without DisconnectNamedPipe keeps a delivered reply readable;
				// an unanswered request simply sees the pipe break, which is indeterminate.
				Pipe = windows_handle{ };
				Lease.reset();
			}
			auto pending() -> bool
			{
				const std::lock_guard lock{ Mutex };
				return Pipe.valid();
			}

		private:
			std::mutex                                          Mutex;
			windows_handle                                      Pipe;
			std::shared_ptr<application::profile_runtime_lease> Lease;
		};

		/// \brief Creates one instance carrying exactly the owner-private descriptor this adapter writes.
		auto create_instance(const std::wstring& aName, const private_security& aSecurity, const bool aFirst) -> windows_handle
		{
			private_security_attributes attributes;
			if (!attributes.initialize(aSecurity, windows_object_kind::regular_file)) {
				return windows_handle{ };
			}
			return windows_handle{ ::CreateNamedPipeW(aName.c_str(),
													  PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (aFirst ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0U),
													  PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
													  pipe_instances, pipe_buffer, pipe_buffer, 0U, attributes.attributes()) };
		}

		class native_server final : public application::profile_activation_server
		{
		public:
			native_server(windows_handle aListener, std::wstring aName, private_security aSecurity, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Listener(std::move(aListener))
				, Name(std::move(aName))
				, Security(std::move(aSecurity))
				, Profile(aProfile)
				, Lease(std::move(aLease))
			{
				arm();
			}
			~native_server() override
			{
				if (auto reply = Pending.lock()) {
					reply->complete(outcome::indeterminate);
				}
				release_listener();
			}
			auto receive() -> std::optional<application::profile_activation_request> override
			{
				if (auto reply = Pending.lock(); reply && reply->pending()) {
					return std::nullopt;
				}
				if (!Listener.valid()) {
					Listener = create_instance(Name, Security, false);
					if (!Listener.valid()) {
						return std::nullopt;
					}
					arm();
				}
				DWORD bytes = 0U;
				DWORD error = ERROR_SUCCESS;
				if (!Connected) {
					const auto connected = Connect.poll(Listener.Value, bytes, error);
					if (connected == pipe_operation::state::pending) {
						return std::nullopt;
					}
					if (connected == pipe_operation::state::failed || !same_user(Listener.Value, true, Security)) {
						drop_client();
						return std::nullopt;
					}
					Connected = true;
					Deadline  = clock_type::now() + deadline_interval;
					Read.reset();
					// One bounded read, one byte wider than a frame so an overlong message is refused.
					if (::ReadFile(Listener.Value, Input.data(), static_cast<DWORD>(Input.size()), nullptr, Read.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING && ::GetLastError() != ERROR_MORE_DATA) {
						drop_client();
						return std::nullopt;
					}
				}
				if (clock_type::now() >= Deadline) {
					drop_client();
					return std::nullopt;
				}
				const auto read = Read.poll(Listener.Value, bytes, error);
				if (read == pipe_operation::state::pending) {
					return std::nullopt;
				}
				frame received{ };
				std::copy_n(Input.begin(), frame_size, received.begin());
				const auto intent = read == pipe_operation::state::complete && bytes == frame_size ? frame_intent(received, Profile) : std::nullopt;
				if (!intent) {
					drop_client();
					return std::nullopt;
				}
				// The connected instance travels with the reply; a fresh instance keeps the name bound.
				auto instance = std::move(Listener);
				Connected     = false;
				Listener      = create_instance(Name, Security, false);
				if (Listener.valid()) {
					arm();
				}
				auto reply = std::make_shared<native_reply>(std::move(instance), Lease);
				Pending    = reply;
				return application::profile_activation_request{ *intent, std::move(reply) };
			}

		private:
			void arm()
			{
				Connected = false;
				Connect.reset();
				if (::ConnectNamedPipe(Listener.Value, Connect.overlapped()) != 0) {
					return;
				}
				const auto error = ::GetLastError();
				if (error == ERROR_PIPE_CONNECTED) {
					(void)::SetEvent(Connect.overlapped()->hEvent);
				} else if (error != ERROR_IO_PENDING) {
					release_listener();
				}
			}
			/// \brief Discards the peer and returns the same instance to waiting, so the name stays bound.
			void drop_client()
			{
				if (!Listener.valid()) {
					return;
				}
				(void)::CancelIoEx(Listener.Value, nullptr);
				(void)::DisconnectNamedPipe(Listener.Value);
				arm();
			}
			void release_listener()
			{
				if (Listener.valid()) {
					(void)::CancelIoEx(Listener.Value, nullptr);
					(void)::DisconnectNamedPipe(Listener.Value);
					Listener = windows_handle{ };
				}
				Connected = false;
			}

			windows_handle                                      Listener;
			std::wstring                                        Name;
			private_security                                    Security;
			core::bpr_profile_id                                Profile;
			std::shared_ptr<application::profile_runtime_lease> Lease;
			pipe_operation                                      Connect;
			pipe_operation                                      Read;
			bool                                                Connected = false;
			std::array<unsigned char, frame_size + 1U>          Input{ };
			clock_type::time_point                              Deadline;
			std::weak_ptr<native_reply>                         Pending;
		};

		/// \brief Opens the owner's pipe, waiting only while the single free instance is busy.
		auto open_endpoint(const std::wstring& aName, clock_type::time_point aDeadline, std::stop_token aStop) -> windows_handle
		{
			while (!aStop.stop_requested() && clock_type::now() < aDeadline) {
				windows_handle pipe{ ::CreateFileW(aName.c_str(), GENERIC_READ | GENERIC_WRITE | READ_CONTROL, 0U, nullptr, OPEN_EXISTING,
												   FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr) };
				if (pipe.valid()) {
					return pipe;
				}
				if (::GetLastError() != ERROR_PIPE_BUSY) {
					return windows_handle{ };
				}
				(void)::WaitNamedPipeW(aName.c_str(), 20U);
			}
			return windows_handle{ };
		}
#endif
	}

	auto profile_activation_endpoint(const std::filesystem::path& aRoot)
		-> std::expected<std::filesystem::path, profile_activation_endpoint_failure>
	{
		try {
			return resolve_endpoint(aRoot);
		} catch (...) {
			return std::unexpected(profile_activation_endpoint_failure::unavailable);
		}
	}

	auto describe_profile_activation_endpoint_failure(const profile_activation_endpoint_failure aFailure) noexcept -> const char*
	{
		switch (aFailure) {
		case profile_activation_endpoint_failure::platform_unsupported:     return "platform-unsupported";
		case profile_activation_endpoint_failure::root_refused:             return "root-refused";
		case profile_activation_endpoint_failure::runtime_directory_absent: return "runtime-directory-absent";
		case profile_activation_endpoint_failure::path_too_long:            return "endpoint-path-too-long";
		case profile_activation_endpoint_failure::unavailable:              break;
		}
		return "unavailable";
	}

	auto make_profile_activation_server(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease)
		-> std::expected<std::shared_ptr<application::profile_activation_server>, outcome>
	{
		try {
			if (aProfile.value() == 0U || !detail::validates_profile_runtime_lease(aRoot, aLease)) {
				return std::unexpected(outcome::unavailable);
			}
			const auto path = resolve_endpoint(aRoot);
			if (!path) {
				return std::unexpected(outcome::unavailable);
			}
#if defined(_WIN32)
			auto security = private_security::load();
			if (!security) {
				return std::unexpected(outcome::unavailable);
			}
			// The first instance is the ownership claim: an existing name, whoever holds
			// it, refuses this owner rather than being joined or replaced.
			auto listener = create_instance(path->native(), *security, true);
			if (!listener.valid()) {
				return std::unexpected(outcome::unavailable);
			}
			return std::make_shared<native_server>(std::move(listener), path->native(), std::move(*security), aProfile, std::move(aLease));
#else
			const auto address = endpoint_address(*path);
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
			// The runtime directory is already owner-private, so no other user can reach the socket before chmod.
			if (::chmod(address->sun_path, 0600) != 0 || ::listen(listener.Value, 8) != 0) {
				(void)::unlink(address->sun_path);
				return std::unexpected(outcome::unavailable);
			}
			return std::make_shared<native_server>(std::move(listener), aRoot, *path, aProfile, std::move(aLease));
#endif
		} catch (...) {
			return std::unexpected(outcome::unavailable);
		}
	}

	auto send_profile_activation(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, core::profile_launch_intent aIntent, std::stop_token aStop)
		-> std::expected<outcome, outcome>
	{
		bool sent = false;
		try {
			if (aStop.stop_requested() || aProfile.value() == 0U || (aIntent != core::profile_launch_intent::activate && aIntent != core::profile_launch_intent::new_window)) {
				return std::unexpected(outcome::unavailable);
			}
			const auto path = resolve_endpoint(aRoot);
			if (!path) {
				return std::unexpected(outcome::unavailable);
			}
#if defined(_WIN32)
			const auto security = private_security::load();
			if (!security) {
				return std::unexpected(outcome::unavailable);
			}
			const auto deadline = clock_type::now() + deadline_interval;
			auto       pipe     = open_endpoint(path->native(), deadline, aStop);
			DWORD      mode     = PIPE_READMODE_MESSAGE;
			// The owner is authenticated before a byte is sent: its process runs as this
			// user and the pipe carries exactly the owner-private descriptor this adapter writes.
			if (!pipe.valid() || !same_user(pipe.Value, false, *security) || !validate_private_security(pipe.Value, *security, windows_object_kind::regular_file)
				|| ::SetNamedPipeHandleState(pipe.Value, &mode, nullptr, nullptr) == 0 || aStop.stop_requested()) {
				return std::unexpected(outcome::unavailable);
			}
			// [[BPR-0980]]. Windows lets the process a person just launched take the
			// foreground, not the owner this request asks to show a window. Hand that
			// right to the authenticated owner, and to nobody else, before asking. A
			// launch that holds no such right has none to hand on; the owner's window
			// then flashes in the taskbar, which is what Windows does for any window
			// raised from the background.
			if (DWORD owner = 0U; ::GetNamedPipeServerProcessId(pipe.Value, &owner) != 0 && owner != 0U) {
				(void)::AllowSetForegroundWindow(owner);
			}
			const auto     request = make_frame(aProfile, aIntent);
			pipe_operation write;
			DWORD          bytes = 0U;
			DWORD          error = ERROR_SUCCESS;
			if (!write.valid() || (::WriteFile(pipe.Value, request.data(), static_cast<DWORD>(request.size()), nullptr, write.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING)) {
				return std::unexpected(outcome::unavailable);
			}
			sent = true;
			if (!write.await(pipe.Value, deadline, aStop, bytes, error) || bytes != request.size()) {
				return std::unexpected(outcome::indeterminate);
			}
			std::array<unsigned char, 2> response{ };
			pipe_operation               read;
			if (!read.valid() || (::ReadFile(pipe.Value, response.data(), static_cast<DWORD>(response.size()), nullptr, read.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING)
				|| !read.await(pipe.Value, deadline, aStop, bytes, error) || bytes != response.size() || response[0] != 1U || (response[1] != 1U && response[1] != 2U)) {
				return std::unexpected(outcome::indeterminate);
			}
			return response[1] == 1U ? outcome::queued : outcome::refused;
#else
			const auto address = endpoint_address(*path);
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
#endif
		} catch (...) {
			return std::unexpected(sent ? outcome::indeterminate : outcome::unavailable);
		}
	}

	namespace {
		using tour_failure                           = application::tour_channel_failure;
		using tour_bytes                             = application::tour_channel_bytes;
		using tour_hello                             = std::array<std::uint8_t, 64>;
		constexpr std::uint8_t tour_protocol_version = 6U;
		auto                   tour_endpoint(const std::filesystem::path& aRoot) -> std::optional<std::filesystem::path>
		{
			auto path = profile_activation_endpoint(aRoot);
			if (!path) {
				return std::nullopt;
			}
			auto name = path->native();
#if defined(_WIN32)
			const auto at = name.find(L"bpr-activate-");
			if (at == std::wstring::npos) {
				return std::nullopt;
			}
			name.replace(at, 13, L"bpr-tourcmd--");
#else
			const auto at = name.find("bpr-activate-");
			if (at == std::string::npos) {
				return std::nullopt;
			}
			name.replace(at, 13, "bpr-tourcmd--");
#endif
			return std::filesystem::path{ std::move(name) };
		}
		auto make_tour_hello(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, bool aOwner) -> tour_hello
		{
			// Version 3 added the finite cycle commands (BPR-1168), version 4 the round grant, read,
			// page, source, response append and policy commands (BPR-1169), version 5 rebind
			// (BPR-1279) and version 6 the tour source read (BPR-1227); either side refuses another
			// version.
			tour_hello result{ 'B', 'P', 'R', 'T', tour_protocol_version, 0, 0, 0 };
			for (unsigned i = 0; i < 8; ++i) {
				result[8 + i] = static_cast<std::uint8_t>(aProfile.value() >> (i * 8));
			}
			const auto root = std::filesystem::canonical(aRoot).native();
			const auto hash = core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(root.data()), root.size() * sizeof(root.front()) });
			std::copy(hash.begin(), hash.end(), result.begin() + 16);
			if (aOwner) {
				std::random_device random;
				for (std::size_t i = 48; i < result.size(); ++i) {
					result[i] = static_cast<std::uint8_t>(random());
				}
			}
			return result;
		}
		void append_size(tour_bytes& aBytes, std::size_t aSize)
		{
			for (unsigned i = 0; i < 8; ++i) {
				aBytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(aSize) >> (i * 8)));
			}
		}
		auto payload_size(std::span<const std::uint8_t> aBytes) -> std::optional<std::size_t>
		{
			if (aBytes.size() < 8) {
				return std::nullopt;
			}
			std::uint64_t size = 0;
			for (unsigned i = 0; i < 8; ++i) {
				size |= static_cast<std::uint64_t>(aBytes[i]) << (i * 8);
			}
			if (size == 0 || size > application::maximum_tour_channel_bytes) {
				return std::nullopt;
			}
			return static_cast<std::size_t>(size);
		}
#if !defined(_WIN32)
		auto write_tour_bytes(int aSocket, std::span<const std::uint8_t> aBytes, clock_type::time_point aDeadline, std::stop_token aStop = { }) -> bool
		{
			while (!aBytes.empty() && wait_socket(aSocket, POLLOUT, aDeadline, aStop)) {
				const auto sent = send_bytes(aSocket, aBytes.data(), aBytes.size());
				if (sent > 0) {
					aBytes = aBytes.subspan(static_cast<std::size_t>(sent));
				} else if (sent == 0 || (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)) {
					return false;
				}
			}
			return aBytes.empty();
		}
		auto read_tour_bytes(int aSocket, std::span<std::uint8_t> aBytes, clock_type::time_point aDeadline, std::stop_token aStop) -> bool
		{
			while (!aBytes.empty() && wait_socket(aSocket, POLLIN, aDeadline, aStop)) {
				const auto read = ::recv(aSocket, aBytes.data(), aBytes.size(), 0);
				if (read > 0) {
					aBytes = aBytes.subspan(static_cast<std::size_t>(read));
				} else if (read == 0 || (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)) {
					return false;
				}
			}
			return aBytes.empty();
		}
		class tour_native_reply final : public application::tour_owner_reply
		{
		public:
			tour_native_reply(socket_handle aSocket, tour_hello aHello, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Socket{ std::move(aSocket) }
				, Hello{ aHello }
				, Lease{ std::move(aLease) }
			{ }
			void complete(std::span<const std::uint8_t> aResponse) noexcept override
			{
				const std::lock_guard lock{ Mutex };
				if (Socket.Value < 0) {
					return;
				}
				try {
					if (!aResponse.empty() && aResponse.size() <= application::maximum_tour_channel_bytes) {
						tour_bytes header{ Hello.begin(), Hello.end() };
						append_size(header, aResponse.size());
						const auto deadline = clock_type::now() + deadline_interval;
						if (write_tour_bytes(Socket.Value, header, deadline)) {
							static_cast<void>(write_tour_bytes(Socket.Value, aResponse, deadline));
						}
					}
				} catch (...) { }
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
			tour_hello                                          Hello;
			std::shared_ptr<application::profile_runtime_lease> Lease;
		};
		class tour_native_server final : public application::tour_owner_server
		{
		public:
			tour_native_server(socket_handle aListener, std::filesystem::path aRoot, std::filesystem::path aPath, tour_hello aHello, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Listener{ std::move(aListener) }
				, Root{ std::move(aRoot) }
				, Path{ std::move(aPath) }
				, Hello{ aHello }
				, Lease{ std::move(aLease) }
			{
				static_cast<void>(::lstat(Path.c_str(), &Identity));
			}
			~tour_native_server() override
			{
				if (auto pending = Pending.lock()) {
					pending->complete({ });
				}
				Listener = socket_handle{ };
				Client   = socket_handle{ };
				struct stat named{ };
				if (detail::validates_profile_runtime_lease(Root, Lease) && ::lstat(Path.c_str(), &named) == 0 && named.st_dev == Identity.st_dev && named.st_ino == Identity.st_ino) {
					static_cast<void>(::unlink(Path.c_str()));
				}
			}
			auto receive() -> std::optional<application::tour_owner_message> override
			{
				if (auto pending = Pending.lock(); pending && pending->pending()) {
					return std::nullopt;
				}
				if (Client.Value < 0) {
					Client = socket_handle{ ::accept(Listener.Value, nullptr, nullptr) };
					if (Client.Value < 0) {
						return std::nullopt;
					}
					Deadline = clock_type::now() + std::chrono::seconds{ 5 };
					Input.clear();
					Expected = 0;
					if (!configure_socket(Client.Value) || !same_user(Client.Value) || !write_tour_bytes(Client.Value, Hello, Deadline)) {
						Client = socket_handle{ };
						return std::nullopt;
					}
				}
				if (clock_type::now() >= Deadline) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				std::array<std::uint8_t, 65536> chunk{ };
				const auto                      read = ::recv(Client.Value, chunk.data(), chunk.size(), 0); // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
				if (read < 0) {
					if (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK) {
						Client = socket_handle{ };
					}
					return std::nullopt;
				}
				if (read > 0) {
					const auto size = static_cast<std::size_t>(read);
					if (Input.size() + size > application::maximum_tour_channel_bytes + Hello.size() + 8) {
						Client = socket_handle{ };
						return std::nullopt;
					}
					Input.insert(Input.end(), chunk.begin(), chunk.begin() + read);
					if (Input.size() >= Hello.size() + 8 && Expected == 0) {
						const auto length = payload_size(std::span{ Input }.subspan(Hello.size(), 8));
						if (!length || !std::equal(Hello.begin(), Hello.end(), Input.begin())) {
							Client = socket_handle{ };
							return std::nullopt;
						}
						Expected = Hello.size() + 8 + *length;
					}
					if (Expected && Input.size() > Expected) {
						Client = socket_handle{ };
					}
					return std::nullopt;
				}
				if (!Expected || Input.size() != Expected) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				tour_bytes payload{ Input.begin() + static_cast<std::ptrdiff_t>(Hello.size() + 8), Input.end() };
				if (!decode_tour_request(payload)) {
					Client = socket_handle{ };
					return std::nullopt;
				}
				auto reply = std::make_shared<tour_native_reply>(std::move(Client), Hello, Lease);
				Pending    = reply;
				return application::tour_owner_message{ std::move(payload), std::move(reply) };
			}

		private:
			socket_handle                                       Listener, Client;
			std::filesystem::path                               Root, Path;
			tour_hello                                          Hello;
			std::shared_ptr<application::profile_runtime_lease> Lease;
			struct stat                                         Identity{ };
			clock_type::time_point                              Deadline;
			tour_bytes                                          Input;
			std::size_t                                         Expected = 0;
			std::weak_ptr<tour_native_reply>                    Pending;
		};
#else
		// [[BPR-1168]]. The Windows owner command endpoint is an owner-private named pipe
		// beside the activation pipe, with the POSIX socket's framing: the owner
		// advertises its 64-byte binding, the caller echoes it with an 8-byte length and
		// the request, and the owner answers the same way. Byte mode, because a request
		// or an answer is far larger than one pipe buffer; the length delimits the frame.
		constexpr DWORD tour_pipe_buffer = 64U * 1024U;

		auto create_tour_instance(const std::wstring& aName, const private_security& aSecurity, const bool aFirst) -> windows_handle
		{
			private_security_attributes attributes;
			if (!attributes.initialize(aSecurity, windows_object_kind::regular_file)) {
				return windows_handle{ };
			}
			return windows_handle{ ::CreateNamedPipeW(aName.c_str(),
													  PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (aFirst ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0U),
													  PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
													  pipe_instances, tour_pipe_buffer, tour_pipe_buffer, 0U, attributes.attributes()) };
		}
		/// \brief Writes every byte before the deadline, or reports that it could not.
		auto write_tour_pipe(HANDLE aPipe, std::span<const std::uint8_t> aBytes, const clock_type::time_point aDeadline, const std::stop_token aStop = { }) -> bool
		{
			while (!aBytes.empty()) {
				pipe_operation write;
				const auto     chunk = static_cast<DWORD>(std::min<std::size_t>(aBytes.size(), tour_pipe_buffer));
				if (!write.valid() || (::WriteFile(aPipe, aBytes.data(), chunk, nullptr, write.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING)) {
					return false;
				}
				DWORD bytes = 0U;
				DWORD error = ERROR_SUCCESS;
				if (!write.await(aPipe, aDeadline, aStop, bytes, error) || bytes == 0U) {
					return false;
				}
				aBytes = aBytes.subspan(bytes);
			}
			return true;
		}
		/// \brief Reads exactly as many bytes as `aBytes` holds before the deadline.
		auto read_tour_pipe(HANDLE aPipe, std::span<std::uint8_t> aBytes, const clock_type::time_point aDeadline, const std::stop_token aStop) -> bool
		{
			while (!aBytes.empty()) {
				pipe_operation read;
				const auto     chunk = static_cast<DWORD>(std::min<std::size_t>(aBytes.size(), tour_pipe_buffer));
				if (!read.valid() || (::ReadFile(aPipe, aBytes.data(), chunk, nullptr, read.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING)) {
					return false;
				}
				DWORD bytes = 0U;
				DWORD error = ERROR_SUCCESS;
				if (!read.await(aPipe, aDeadline, aStop, bytes, error) || bytes == 0U) {
					return false;
				}
				aBytes = aBytes.subspan(bytes);
			}
			return true;
		}
		class tour_native_reply final : public application::tour_owner_reply
		{
		public:
			tour_native_reply(windows_handle aPipe, tour_hello aHello, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Pipe{ std::move(aPipe) }
				, Hello{ aHello }
				, Lease{ std::move(aLease) }
			{ }
			void complete(std::span<const std::uint8_t> aResponse) noexcept override
			{
				const std::lock_guard lock{ Mutex };
				if (!Pipe.valid()) {
					return;
				}
				try {
					if (!aResponse.empty() && aResponse.size() <= application::maximum_tour_channel_bytes) {
						tour_bytes header{ Hello.begin(), Hello.end() };
						append_size(header, aResponse.size());
						// Bounded: a caller that stops reading cannot hold the owner.
						const auto deadline = clock_type::now() + std::chrono::seconds{ 10 };
						if (write_tour_pipe(Pipe.Value, header, deadline)) {
							static_cast<void>(write_tour_pipe(Pipe.Value, aResponse, deadline));
						}
					}
				} catch (...) { }
				// Closing without DisconnectNamedPipe keeps a written answer readable; an
				// unanswered request sees the pipe break, which is indeterminate.
				Pipe = windows_handle{ };
				Lease.reset();
			}
			auto pending() -> bool
			{
				const std::lock_guard lock{ Mutex };
				return Pipe.valid();
			}

		private:
			std::mutex                                          Mutex;
			windows_handle                                      Pipe;
			tour_hello                                          Hello;
			std::shared_ptr<application::profile_runtime_lease> Lease;
		};
		class tour_native_server final : public application::tour_owner_server
		{
		public:
			tour_native_server(windows_handle aListener, std::wstring aName, private_security aSecurity, tour_hello aHello, std::shared_ptr<application::profile_runtime_lease> aLease)
				: Listener{ std::move(aListener) }
				, Name{ std::move(aName) }
				, Security{ std::move(aSecurity) }
				, Hello{ aHello }
				, Lease{ std::move(aLease) }
			{
				arm();
			}
			~tour_native_server() override
			{
				if (auto pending = Pending.lock()) {
					pending->complete({ });
				}
				release_listener();
			}
			tour_native_server(const tour_native_server&)                    = delete;
			auto operator=(const tour_native_server&) -> tour_native_server& = delete;
			auto receive() -> std::optional<application::tour_owner_message> override
			{
				if (auto pending = Pending.lock(); pending && pending->pending()) {
					return std::nullopt;
				}
				if (!Listener.valid()) {
					Listener = create_tour_instance(Name, Security, false);
					if (!Listener.valid()) {
						return std::nullopt;
					}
					arm();
				}
				DWORD bytes = 0U;
				DWORD error = ERROR_SUCCESS;
				if (!Connected) {
					const auto connected = Connect.poll(Listener.Value, bytes, error);
					if (connected == pipe_operation::state::pending) {
						return std::nullopt;
					}
					if (connected == pipe_operation::state::failed || !same_user(Listener.Value, true, Security)) {
						drop_client();
						return std::nullopt;
					}
					Connected = true;
					Reading   = false;
					Deadline  = clock_type::now() + std::chrono::seconds{ 5 };
					Input.clear();
					Expected = 0;
					// The owner advertises its binding first; the caller sends nothing until it matches.
					if (!write_tour_pipe(Listener.Value, Hello, Deadline)) {
						drop_client();
						return std::nullopt;
					}
				}
				if (clock_type::now() >= Deadline) {
					drop_client();
					return std::nullopt;
				}
				if (!Reading) {
					Read.reset();
					if (::ReadFile(Listener.Value, Chunk.data(), static_cast<DWORD>(Chunk.size()), nullptr, Read.overlapped()) == 0 && ::GetLastError() != ERROR_IO_PENDING) {
						drop_client();
						return std::nullopt;
					}
					Reading = true;
				}
				const auto read = Read.poll(Listener.Value, bytes, error);
				if (read == pipe_operation::state::pending) {
					return std::nullopt;
				}
				Reading = false;
				if (read == pipe_operation::state::failed || bytes == 0U || Input.size() + bytes > application::maximum_tour_channel_bytes + Hello.size() + 8) {
					drop_client();
					return std::nullopt;
				}
				Input.insert(Input.end(), Chunk.begin(), Chunk.begin() + bytes);
				if (Input.size() >= Hello.size() + 8 && Expected == 0) {
					const auto length = payload_size(std::span{ Input }.subspan(Hello.size(), 8));
					if (!length || !std::equal(Hello.begin(), Hello.end(), Input.begin())) {
						drop_client();
						return std::nullopt;
					}
					Expected = Hello.size() + 8 + *length;
				}
				if (Expected == 0 || Input.size() < Expected) {
					return std::nullopt;
				}
				if (Input.size() > Expected) {
					drop_client();
					return std::nullopt;
				}
				tour_bytes payload{ Input.begin() + static_cast<std::ptrdiff_t>(Hello.size() + 8), Input.end() };
				if (!decode_tour_request(payload)) {
					drop_client();
					return std::nullopt;
				}
				// The connected instance travels with the reply; a fresh instance keeps the name bound.
				auto instance = std::move(Listener);
				Connected     = false;
				Listener      = create_tour_instance(Name, Security, false);
				if (Listener.valid()) {
					arm();
				}
				auto reply = std::make_shared<tour_native_reply>(std::move(instance), Hello, Lease);
				Pending    = reply;
				return application::tour_owner_message{ std::move(payload), std::move(reply) };
			}

		private:
			void arm()
			{
				Connected = false;
				Reading   = false;
				Connect.reset();
				if (::ConnectNamedPipe(Listener.Value, Connect.overlapped()) != 0) {
					return;
				}
				const auto error = ::GetLastError();
				if (error == ERROR_PIPE_CONNECTED) {
					(void)::SetEvent(Connect.overlapped()->hEvent);
				} else if (error != ERROR_IO_PENDING) {
					release_listener();
				}
			}
			/// \brief Discards the peer and returns the same instance to waiting, so the name stays bound.
			void drop_client()
			{
				if (!Listener.valid()) {
					return;
				}
				(void)::CancelIoEx(Listener.Value, nullptr);
				DWORD ignored = 0U;
				if (Reading) {
					(void)::GetOverlappedResult(Listener.Value, Read.overlapped(), &ignored, TRUE);
				}
				(void)::DisconnectNamedPipe(Listener.Value);
				arm();
			}
			void release_listener()
			{
				if (Listener.valid()) {
					(void)::CancelIoEx(Listener.Value, nullptr);
					DWORD ignored = 0U;
					if (Reading) {
						(void)::GetOverlappedResult(Listener.Value, Read.overlapped(), &ignored, TRUE);
					}
					(void)::DisconnectNamedPipe(Listener.Value);
					Listener = windows_handle{ };
				}
				Connected = false;
				Reading   = false;
			}

			windows_handle                                      Listener;
			std::wstring                                        Name;
			private_security                                    Security;
			tour_hello                                          Hello;
			std::shared_ptr<application::profile_runtime_lease> Lease;
			pipe_operation                                      Connect;
			pipe_operation                                      Read;
			bool                                                Connected = false;
			bool                                                Reading   = false;
			clock_type::time_point                              Deadline;
			tour_bytes                                          Input;
			std::size_t                                         Expected = 0;
			std::array<std::uint8_t, tour_pipe_buffer>          Chunk{ };
			std::weak_ptr<tour_native_reply>                    Pending;
		};
#endif
	}
	auto make_tour_owner_server(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::shared_ptr<application::profile_runtime_lease> aLease) -> std::expected<std::shared_ptr<application::tour_owner_server>, application::tour_channel_failure>
	{
		try {
			if (!aProfile.value() || !detail::validates_profile_runtime_lease(aRoot, aLease)) {
				return std::unexpected(tour_failure::refused);
			}
			const auto path = tour_endpoint(aRoot);
			if (!path) {
				return std::unexpected(tour_failure::unavailable);
			}
#if defined(_WIN32)
			auto security = private_security::load();
			if (!security) {
				return std::unexpected(tour_failure::unavailable);
			}
			// The first instance is the claim: an existing name, whoever holds it, refuses this owner.
			auto listener = create_tour_instance(path->native(), *security, true);
			if (!listener.valid()) {
				return std::unexpected(tour_failure::unavailable);
			}
			return std::make_shared<tour_native_server>(std::move(listener), path->native(), std::move(*security), make_tour_hello(aRoot, aProfile, true), std::move(aLease));
#else
			const auto address = endpoint_address(*path);
			if (!address) {
				return std::unexpected(tour_failure::unavailable);
			}
			struct stat existing{ };
			if (::lstat(path->c_str(), &existing) == 0) {
				if (!private_socket(path->c_str())) {
					return std::unexpected(tour_failure::refused);
				}
				socket_handle probe{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
				if (!configure_socket(probe.Value) || ::connect(probe.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) == 0 || errno != ECONNREFUSED || ::unlink(path->c_str()) != 0) {
					return std::unexpected(tour_failure::refused);
				}
			} else if (errno != ENOENT) {
				return std::unexpected(tour_failure::refused);
			}
			socket_handle listener{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
			if (!configure_socket(listener.Value) || ::bind(listener.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) != 0) {
				return std::unexpected(tour_failure::unavailable);
			}
			if (::chmod(path->c_str(), 0600) != 0 || ::listen(listener.Value, 8) != 0) {
				static_cast<void>(::unlink(path->c_str()));
				return std::unexpected(tour_failure::unavailable);
			}
			return std::make_shared<tour_native_server>(std::move(listener), aRoot, *path, make_tour_hello(aRoot, aProfile, true), std::move(aLease));
#endif
		} catch (...) {
			return std::unexpected(tour_failure::unavailable);
		}
	}
	auto tour_owner_endpoint(const std::filesystem::path& aRoot) -> std::optional<std::filesystem::path>
	{
		try {
			return tour_endpoint(aRoot);
		} catch (...) {
			return std::nullopt;
		}
	}
	auto send_tour_owner(const std::filesystem::path& aRoot, core::bpr_profile_id aProfile, std::span<const std::uint8_t> aPayload, std::stop_token aStop, std::chrono::seconds aAnswerWait)
		-> std::expected<application::tour_channel_bytes, application::tour_channel_failure>
	{
		bool sent = false;
		try {
			if (aStop.stop_requested()) {
				return std::unexpected(tour_failure::cancelled);
			}
			if (aPayload.empty() || aPayload.size() > application::maximum_tour_channel_bytes || !decode_tour_request(aPayload)) {
				return std::unexpected(tour_failure::refused);
			}
			const auto path = tour_endpoint(aRoot);
			if (!path) {
				return std::unexpected(tour_failure::unavailable);
			}
#if defined(_WIN32)
			const auto security = private_security::load();
			if (!security) {
				return std::unexpected(tour_failure::unavailable);
			}
			const auto deadline = clock_type::now() + std::chrono::seconds{ 10 };
			auto       pipe     = open_endpoint(path->native(), deadline, aStop);
			if (!pipe.valid()) {
				return std::unexpected(aStop.stop_requested() ? tour_failure::cancelled : tour_failure::unavailable);
			}
			// The owner is authenticated before a byte is sent: it runs as this user and its
			// pipe carries exactly the owner-private descriptor this adapter writes.
			if (!same_user(pipe.Value, false, *security) || !validate_private_security(pipe.Value, *security, windows_object_kind::regular_file)) {
				return std::unexpected(tour_failure::refused);
			}
			tour_hello hello{ };
			const auto expected = make_tour_hello(aRoot, aProfile, false);
			if (!read_tour_pipe(pipe.Value, hello, deadline, aStop)) {
				return std::unexpected(tour_failure::refused);
			}
			if (std::equal(expected.begin(), expected.begin() + 4, hello.begin()) && hello[4] != expected[4]) {
				return std::unexpected(tour_failure::version_mismatch);
			}
			if (!std::equal(expected.begin(), expected.begin() + 48, hello.begin())) {
				return std::unexpected(tour_failure::refused);
			}
			tour_bytes header{ hello.begin(), hello.end() };
			append_size(header, aPayload.size());
			sent = true;
			if (!write_tour_pipe(pipe.Value, header, deadline, aStop) || !write_tour_pipe(pipe.Value, aPayload, deadline, aStop)) {
				return std::unexpected(tour_failure::indeterminate);
			}
			const auto                   answered = clock_type::now() + aAnswerWait;
			std::array<std::uint8_t, 72> response{ };
			if (!read_tour_pipe(pipe.Value, response, answered, aStop) || !std::equal(hello.begin(), hello.end(), response.begin())) {
				return std::unexpected(tour_failure::indeterminate);
			}
			const auto size = payload_size(std::span{ response }.subspan(64));
			if (!size) {
				return std::unexpected(tour_failure::indeterminate);
			}
			tour_bytes result(*size);
			if (!read_tour_pipe(pipe.Value, result, answered, aStop) || !decode_tour_response(result)) {
				return std::unexpected(tour_failure::indeterminate);
			}
			return result;
#else
			const auto address = endpoint_address(*path);
			if (!address || !private_socket(path->c_str())) {
				return std::unexpected(tour_failure::unavailable);
			}
			socket_handle socket{ ::socket(AF_UNIX, SOCK_STREAM, 0) };
			if (!configure_socket(socket.Value)) {
				return std::unexpected(tour_failure::unavailable);
			}
			const auto deadline = clock_type::now() + std::chrono::seconds{ 10 };
			if (::connect(socket.Value, reinterpret_cast<const sockaddr*>(&*address), sizeof(*address)) != 0) {
				if (errno != EINPROGRESS || !wait_socket(socket.Value, POLLOUT, deadline, aStop)) {
					return std::unexpected(tour_failure::unavailable);
				}
				int       error = 0;
				socklen_t size  = sizeof(error);
				if (::getsockopt(socket.Value, SOL_SOCKET, SO_ERROR, &error, &size) != 0 || error) {
					return std::unexpected(tour_failure::unavailable);
				}
			}
			if (!same_user(socket.Value)) {
				return std::unexpected(tour_failure::refused);
			}
			tour_hello hello{ };
			const auto expected = make_tour_hello(aRoot, aProfile, false);
			if (!read_tour_bytes(socket.Value, hello, deadline, aStop)) {
				return std::unexpected(tour_failure::refused);
			}
			if (std::equal(expected.begin(), expected.begin() + 4, hello.begin()) && hello[4] != expected[4]) {
				// An older or newer owner: say so, and send it nothing.
				return std::unexpected(tour_failure::version_mismatch);
			}
			if (!std::equal(expected.begin(), expected.begin() + 48, hello.begin())) {
				return std::unexpected(tour_failure::refused);
			}
			tour_bytes header{ hello.begin(), hello.end() };
			append_size(header, aPayload.size());
			sent = true;
			if (!write_tour_bytes(socket.Value, header, deadline, aStop) || !write_tour_bytes(socket.Value, aPayload, deadline, aStop) || ::shutdown(socket.Value, SHUT_WR) != 0) {
				return std::unexpected(tour_failure::indeterminate);
			}
			// The owner may take as long as the command does; the caller bounds its own wait.
			const auto                   answered = clock_type::now() + aAnswerWait;
			std::array<std::uint8_t, 72> response{ };
			if (!read_tour_bytes(socket.Value, response, answered, aStop) || !std::equal(hello.begin(), hello.end(), response.begin())) {
				return std::unexpected(tour_failure::indeterminate);
			}
			const auto size = payload_size(std::span{ response }.subspan(64));
			if (!size) {
				return std::unexpected(tour_failure::indeterminate);
			}
			tour_bytes result(*size);
			if (!read_tour_bytes(socket.Value, result, answered, aStop) || !decode_tour_response(result)) {
				return std::unexpected(tour_failure::indeterminate);
			}
			return result;
#endif
		} catch (...) {
			return std::unexpected(sent ? tour_failure::indeterminate : tour_failure::unavailable);
		}
	}

}
