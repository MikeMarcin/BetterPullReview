#include "designated_live_public_observer.hpp"

#include "designated_live_macos_provenance.hpp"
#include "designated_live_macos_supervisor_identity.hpp"
#include "designated_live_macos_supervisor_launch.hpp"
#include <CoreFoundation/CoreFoundation.h>

#include <Security/Security.h>
#include <mach-o/dyld.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
	constexpr auto helper_suffix = std::string_view{
		"/BetterPullReview.app/Contents/Helpers/"
		"better_pull_review_designated_live_supervisor"
	};
	constexpr auto application_relative
		= bpr::app::designated_live_supervisor_launch::application_relative;
	constexpr auto source_sha256   = std::string_view{ BPR_ATTENDED_REVIEW_SOURCE_SHA256 };
	constexpr auto verifier_sha256 = std::string_view{
		BPR_DESIGNATED_LIVE_VERIFIER_SHA256
	};
	constexpr auto child_channel_descriptor = 3;
	constexpr auto child_probe_argument     = std::string_view{
		"--designated-live-preflight-probe"
	};
	constexpr auto child_probe_acknowledgement = std::string_view{
		"BPR_DESIGNATED_LIVE_PREFLIGHT_OK\n"
	};

	class file_descriptor final
	{
	public:
		explicit file_descriptor(const int aValue = -1) noexcept
			: Value{ aValue }
		{
		}
		~file_descriptor()
		{
			if (Value >= 0) {
				static_cast<void>(::close(Value));
			}
		}
		file_descriptor(const file_descriptor&)                    = delete;
		auto operator=(const file_descriptor&) -> file_descriptor& = delete;
		file_descriptor(file_descriptor&& aOther) noexcept
			: Value{ std::exchange(aOther.Value, -1) }
		{
		}
		auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
		{
			if (this != &aOther) {
				if (Value >= 0) {
					static_cast<void>(::close(Value));
				}
				Value = std::exchange(aOther.Value, -1);
			}
			return *this;
		}
		[[nodiscard]] auto     get() const noexcept -> int { return Value; }
		[[nodiscard]] explicit operator bool() const noexcept { return Value >= 0; }

	private:
		int Value;
	};

	template <typename Type>
	class cf_reference final
	{
	public:
		explicit cf_reference(Type aValue = nullptr) noexcept
			: Value{ aValue }
		{
		}
		~cf_reference()
		{
			if (Value != nullptr) {
				CFRelease(Value);
			}
		}
		cf_reference(const cf_reference&)                    = delete;
		auto operator=(const cf_reference&) -> cf_reference& = delete;
		cf_reference(cf_reference&& aOther) noexcept
			: Value{ std::exchange(aOther.Value, nullptr) }
		{
		}
		[[nodiscard]] auto     get() const noexcept -> Type { return Value; }
		[[nodiscard]] explicit operator bool() const noexcept
		{
			return Value != nullptr;
		}

	private:
		Type Value;
	};

	[[nodiscard]] auto lowercase_sha256(const std::string_view aValue) noexcept
		-> bool
	{
		if (aValue.size() != 64U) {
			return false;
		}
		for (const auto character : aValue) {
			if (!((character >= '0' && character <= '9')
				  || (character >= 'a' && character <= 'f'))) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto running_executable() -> std::optional<std::filesystem::path>
	{
		auto required = std::uint32_t{ 0U };
		static_cast<void>(::_NSGetExecutablePath(nullptr, &required));
		if (required == 0U) {
			return std::nullopt;
		}
		auto bytes = std::vector<char>(required, '\0');
		if (::_NSGetExecutablePath(bytes.data(), &required) != 0) {
			return std::nullopt;
		}
		std::error_code error;
		auto            result = std::filesystem::canonical(bytes.data(), error);
		return error || !result.is_absolute() ? std::nullopt
											  : std::optional{ std::move(result) };
	}

	[[nodiscard]] auto capsule_root(const std::filesystem::path& aExecutable)
		-> std::optional<std::filesystem::path>
	{
		const auto text = aExecutable.generic_string();
		if (!text.ends_with(helper_suffix) || text.size() <= helper_suffix.size()) {
			return std::nullopt;
		}
		auto root = std::filesystem::path{
			text.substr(0U, text.size() - helper_suffix.size())
		};
		if (root.empty() || root.lexically_normal() != root) {
			return std::nullopt;
		}
		return root;
	}

	[[nodiscard]] auto signing_digest(SecStaticCodeRef aCode)
		-> std::optional<std::vector<std::uint8_t>>
	{
		CFDictionaryRef informationRaw = nullptr;
		if (SecCodeCopySigningInformation(
				aCode, kSecCSSigningInformation, &informationRaw)
			!= errSecSuccess) {
			return std::nullopt;
		}
		auto  information = cf_reference<CFDictionaryRef>{ informationRaw };
		auto* value       = CFDictionaryGetValue(information.get(), kSecCodeInfoUnique);
		if (value == nullptr || CFGetTypeID(value) != CFDataGetTypeID()) {
			return std::nullopt;
		}
		auto* data = static_cast<CFDataRef>(value);
		return std::vector<std::uint8_t>{
			CFDataGetBytePtr(data),
			CFDataGetBytePtr(data) + CFDataGetLength(data),
		};
	}

	[[nodiscard]] auto verify_running_self(
		const std::filesystem::path& aExpectedPath) -> bool
	{
		SecCodeRef selfRaw = nullptr;
		if (SecCodeCopySelf(kSecCSDefaultFlags, &selfRaw) != errSecSuccess) {
			return false;
		}
		auto self = cf_reference<SecCodeRef>{ selfRaw };
		if (SecCodeCheckValidity(self.get(), kSecCSStrictValidate, nullptr)
			!= errSecSuccess) {
			return false;
		}
		CFURLRef pathRaw = nullptr;
		if (SecCodeCopyPath(self.get(), kSecCSDefaultFlags, &pathRaw)
			!= errSecSuccess) {
			return false;
		}
		auto path   = cf_reference<CFURLRef>{ pathRaw };
		auto buffer = std::array<std::uint8_t, PATH_MAX>{ };
		if (!CFURLGetFileSystemRepresentation(
				path.get(), true, buffer.data(), buffer.size())) {
			return false;
		}
		std::error_code error;
		const auto      observed = std::filesystem::canonical(
			reinterpret_cast<const char*>(buffer.data()), error);
		return !error && observed == aExpectedPath;
	}

	[[nodiscard]] auto static_signing_digest(const std::filesystem::path& aPath)
		-> std::optional<std::vector<std::uint8_t>>
	{
		auto url = cf_reference<CFURLRef>{ CFURLCreateFromFileSystemRepresentation(
			kCFAllocatorDefault,
			reinterpret_cast<const UInt8*>(aPath.c_str()),
			static_cast<CFIndex>(aPath.string().size()),
			false) };
		if (!url) {
			return std::nullopt;
		}
		SecStaticCodeRef codeRaw = nullptr;
		if (SecStaticCodeCreateWithPath(url.get(), kSecCSDefaultFlags, &codeRaw)
				!= errSecSuccess
			|| codeRaw == nullptr) {
			return std::nullopt;
		}
		auto code = cf_reference<SecStaticCodeRef>{ codeRaw };
		if (SecStaticCodeCheckValidity(code.get(), kSecCSStrictValidate, nullptr)
			!= errSecSuccess) {
			return std::nullopt;
		}
		return signing_digest(code.get());
	}

	[[nodiscard]] auto guest_signing_digest(const pid_t aProcess)
		-> std::optional<std::vector<std::uint8_t>>
	{
		auto processNumber = cf_reference<CFNumberRef>{ CFNumberCreate(
			kCFAllocatorDefault, kCFNumberIntType, &aProcess) };
		auto attributes    = cf_reference<CFMutableDictionaryRef>{
			CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				1,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks)
		};
		if (!processNumber || !attributes) {
			return std::nullopt;
		}
		CFDictionarySetValue(
			attributes.get(), kSecGuestAttributePid, processNumber.get());
		SecCodeRef guestRaw = nullptr;
		if (SecCodeCopyGuestWithAttributes(
				nullptr,
				attributes.get(),
				kSecCSDefaultFlags,
				&guestRaw)
				!= errSecSuccess
			|| guestRaw == nullptr) {
			return std::nullopt;
		}
		auto guest = cf_reference<SecCodeRef>{ guestRaw };
		if (SecCodeCheckValidity(guest.get(), kSecCSStrictValidate, nullptr)
			!= errSecSuccess) {
			return std::nullopt;
		}
		return signing_digest(reinterpret_cast<SecStaticCodeRef>(guest.get()));
	}

	[[nodiscard]] auto wait_success(const pid_t aProcess) noexcept -> bool
	{
		int status = 0;
		for (;;) {
			const auto waited = ::waitpid(aProcess, &status, 0);
			if (waited < 0 && errno == EINTR) {
				continue;
			}
			return waited == aProcess && WIFEXITED(status)
				&& WEXITSTATUS(status) == EXIT_SUCCESS;
		}
	}

	[[nodiscard]] auto run_static_verifier(
		const bpr::app::designated_live_supervisor_identity::session& aSession)
		-> bool
	{
		return bpr::app::designated_live_supervisor_launch::verify_static(
				   aSession,
				   source_sha256,
				   bpr::app::designated_live_supervisor_launch::verifier_operations::production())
			.accepted();
	}

	[[nodiscard]] auto exact_file_identity(
		const int aParent, const std::string_view aName, const struct stat& aExpected)
		-> bool
	{
		struct stat observed{ };
		return ::fstatat(
				   aParent,
				   std::string{ aName }.c_str(),
				   &observed,
				   AT_SYMLINK_NOFOLLOW)
			== 0
			&& observed.st_dev == aExpected.st_dev
			&& observed.st_ino == aExpected.st_ino
			&& observed.st_size == aExpected.st_size
			&& observed.st_mtimespec.tv_sec == aExpected.st_mtimespec.tv_sec
			&& observed.st_mtimespec.tv_nsec == aExpected.st_mtimespec.tv_nsec;
	}

	[[nodiscard]] auto launch_harmless_probe(
		const bpr::app::designated_live_supervisor_identity::session& aSession)
		-> bool
	{
		if (!aSession.revalidate()) {
			return false;
		}
		const auto application         = aSession.root_path() / application_relative;
		const auto relativeApplication = std::filesystem::path{ application_relative };
		auto       parent              = file_descriptor{ ::openat(
			aSession.root_descriptor(),
			relativeApplication.parent_path().c_str(),
			O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
		if (!parent) {
			return false;
		}
		auto        executable = file_descriptor{ ::openat(
			parent.get(),
			application.filename().c_str(),
			O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
		struct stat identity{ };
		if (!parent || !executable || ::fstat(executable.get(), &identity) != 0
			|| !S_ISREG(identity.st_mode) || identity.st_uid != ::geteuid()
			|| (static_cast<unsigned>(identity.st_mode) & 0022U) != 0) {
			return false;
		}
		const auto expectedSigningDigest = static_signing_digest(application);
		if (!expectedSigningDigest.has_value()) {
			return false;
		}
		auto sockets = std::array<int, 2>{ -1, -1 };
		if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets.data()) != 0) {
			return false;
		}
		auto parentSocket = file_descriptor{ sockets[0] };
		auto childSocket  = file_descriptor{ sockets[1] };
		if (::fcntl(parentSocket.get(), F_SETFD, FD_CLOEXEC) != 0
			|| ::fcntl(childSocket.get(), F_SETFD, FD_CLOEXEC) != 0) {
			return false;
		}
		posix_spawn_file_actions_t actions{ };
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return false;
		}
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			return false;
		}
		auto cleanActions = [&] {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			static_cast<void>(posix_spawnattr_destroy(&attributes));
		};
		if (!aSession.configure_application_spawn(actions)
			|| posix_spawn_file_actions_adddup2(
				   &actions, childSocket.get(), child_channel_descriptor)
				!= 0) {
			cleanActions();
			return false;
		}
		sigset_t emptySet{ };
		sigset_t defaultSet{ };
		static_cast<void>(sigemptyset(&emptySet));
		static_cast<void>(sigfillset(&defaultSet));
		const auto flags = static_cast<short>(
			POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_START_SUSPENDED
			| POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
		if (posix_spawnattr_setflags(&attributes, flags) != 0
			|| posix_spawnattr_setsigmask(&attributes, &emptySet) != 0
			|| posix_spawnattr_setsigdefault(&attributes, &defaultSet) != 0) {
			cleanActions();
			return false;
		}
		auto executableArgument = relativeApplication.string();
		auto probeArgument      = std::string{ child_probe_argument };
		auto arguments          = std::array<char*, 3>{
			executableArgument.data(), probeArgument.data(), nullptr
		};
		auto pathValue   = std::array{ 'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/',
									   'b', 'i', 'n', ':', '/', 'b', 'i', 'n', ':', '/', 'u', 's', 'r', '/', 's',
									   'b', 'i', 'n', ':', '/', 's', 'b', 'i', 'n', '\0' };
		auto langValue   = std::array{ 'L', 'A', 'N', 'G', '=', 'C', '\0' };
		auto localeValue = std::array{ 'L', 'C', '_', 'A', 'L', 'L', '=', 'C', '\0' };
		auto environment = std::array<char*, 4>{
			pathValue.data(),
			langValue.data(),
			localeValue.data(),
			nullptr,
		};
		const auto challenge = std::string{ source_sha256 } + "\n";
		auto       launchOperations
			= bpr::app::designated_live_supervisor_launch::operations::production(
				[&](const pid_t aProcess) {
					if (!exact_file_identity(
							parent.get(),
							application.filename().string(),
							identity)) {
						return false;
					}
					const auto childSigningDigest = guest_signing_digest(aProcess);
					return childSigningDigest.has_value()
						&& *childSigningDigest == *expectedSigningDigest
						&& ::write(
							   parentSocket.get(), challenge.data(), challenge.size())
						== static_cast<ssize_t>(challenge.size());
				});
		const auto launched
			= bpr::app::designated_live_supervisor_launch::launch(
				aSession,
				relativeApplication,
				actions,
				attributes,
				arguments.data(),
				environment.data(),
				std::move(launchOperations));
		cleanActions();
		childSocket = file_descriptor{ };
		if (!launched.Spawned || !launched.Resumed) {
			return false;
		}
		const auto process   = launched.Process;
		auto       failChild = [&] {
			static_cast<void>(::kill(process, SIGKILL));
			int status = 0;
			while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
			}
			return false;
		};
		auto descriptor = pollfd{
			.fd = parentSocket.get(), .events = POLLIN, .revents = 0
		};
		if (::poll(&descriptor, 1, 10'000) <= 0
			|| (static_cast<unsigned>(descriptor.revents) & static_cast<unsigned>(POLLIN)) == 0) {
			return failChild();
		}
		auto       response = std::array<char, 64>{ };
		const auto count    = ::read(parentSocket.get(), response.data(), response.size());
		if (count != static_cast<ssize_t>(child_probe_acknowledgement.size())
			|| std::string_view{ response.data(), static_cast<std::size_t>(count) }
				!= child_probe_acknowledgement) {
			return failChild();
		}
		return wait_success(process);
	}

	[[nodiscard]] auto launch_attended_application(
		const std::filesystem::path&                                  aApproval,
		const std::string&                                            aProfile,
		const bpr::app::designated_live_supervisor_identity::session& aSession)
		-> bool
	{
		const auto inventoryDigest = aSession.inventory_sha256();
		if (!aApproval.is_absolute() || aApproval.lexically_normal() != aApproval
			|| inventoryDigest.size() != 64U || !aSession.revalidate()) {
			return false;
		}
		const auto application         = aSession.root_path() / application_relative;
		const auto relativeApplication = std::filesystem::path{ application_relative };
		auto       parent              = file_descriptor{ ::openat(
			aSession.root_descriptor(),
			relativeApplication.parent_path().c_str(),
			O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
		if (!parent) {
			return false;
		}
		auto        executable = file_descriptor{ ::openat(
			parent.get(),
			application.filename().c_str(),
			O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
		struct stat identity{ };
		if (!parent || !executable || ::fstat(executable.get(), &identity) != 0
			|| !S_ISREG(identity.st_mode) || identity.st_uid != ::geteuid()
			|| (static_cast<unsigned>(identity.st_mode) & 0022U) != 0) {
			return false;
		}
		const auto expectedSigningDigest = static_signing_digest(application);
		if (!expectedSigningDigest.has_value()) {
			return false;
		}
		posix_spawn_file_actions_t actions{ };
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return false;
		}
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			return false;
		}
		sigset_t emptySet{ };
		sigset_t defaultSet{ };
		static_cast<void>(sigemptyset(&emptySet));
		static_cast<void>(sigfillset(&defaultSet));
		const auto flags = static_cast<short>(
			POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_START_SUSPENDED
			| POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
		if (posix_spawnattr_setflags(&attributes, flags) != 0
			|| posix_spawnattr_setsigmask(&attributes, &emptySet) != 0
			|| posix_spawnattr_setsigdefault(&attributes, &defaultSet) != 0
			|| !aSession.configure_application_spawn(actions)) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			static_cast<void>(posix_spawnattr_destroy(&attributes));
			return false;
		}
		auto executableArgument = relativeApplication.string();
		auto approvalFlag       = std::string{ "--attended-designated-live-approval" };
		auto approvalArgument   = aApproval.string();
		auto inventoryFlag      = std::string{
			"--attended-designated-live-inventory-sha256"
		};
		auto inventoryArgument = std::string{ inventoryDigest };
		auto profileFlag       = std::string{ "--bpr-profile" };
		auto profileArgument   = aProfile;
		auto arguments         = std::array<char*, 8>{
			executableArgument.data(),
			approvalFlag.data(),
			approvalArgument.data(),
			inventoryFlag.data(),
			inventoryArgument.data(),
			profileFlag.data(),
			profileArgument.data(),
			nullptr,
		};
		auto pathValue   = std::array{ 'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/',
									   'b', 'i', 'n', ':', '/', 'b', 'i', 'n', ':', '/', 'u', 's', 'r', '/', 's',
									   'b', 'i', 'n', ':', '/', 's', 'b', 'i', 'n', '\0' };
		auto langValue   = std::array{ 'L', 'A', 'N', 'G', '=', 'C', '\0' };
		auto localeValue = std::array{ 'L', 'C', '_', 'A', 'L', 'L', '=', 'C', '\0' };
		auto environment = std::array<char*, 4>{
			pathValue.data(), langValue.data(), localeValue.data(), nullptr
		};
		auto channel = std::optional<
			bpr::app::designated_live_provenance::supervisor_channel>{ };
		auto restored = false;
		auto launchOperations
			= bpr::app::designated_live_supervisor_launch::operations::production(
				[&](const pid_t aProcess) {
					if (!restored || !channel.has_value()
						|| !exact_file_identity(
							parent.get(),
							application.filename().string(),
							identity)) {
						return false;
					}
					const auto childSigningDigest = guest_signing_digest(aProcess);
					return childSigningDigest.has_value()
						&& *childSigningDigest == *expectedSigningDigest;
				});
		auto originalSpawn = std::move(launchOperations.Spawn);
		launchOperations.Spawn
			= [&, productionSpawn = std::move(originalSpawn)](
				  pid_t*                            aProcess,
				  const char*                       aExecutable,
				  const posix_spawn_file_actions_t* aActions,
				  const posix_spawnattr_t*          aAttributes,
				  char* const                       aArguments[],
				  char* const                       aEnvironment[]) mutable {
				  // fact_3. Publish the right only for the one spawn that inherits
				  // it. An occupied slot refuses without attempting the spawn.
				  channel = bpr::app::designated_live_provenance::supervisor_channel::claim();
				  if (!channel.has_value()) {
					  return EBUSY;
				  }
				  const auto spawned = productionSpawn(
					  aProcess,
					  aExecutable,
					  aActions,
					  aAttributes,
					  aArguments,
					  aEnvironment);
				  // Immediately, whether or not spawn worked. If a child exists and
				  // restoration fails, PostSpawnCheck routes it through kill/reap.
				  restored = channel->restore();
				  return spawned;
			  };
		const auto launched
			= bpr::app::designated_live_supervisor_launch::launch(
				aSession,
				relativeApplication,
				actions,
				attributes,
				arguments.data(),
				environment.data(),
				std::move(launchOperations));
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		if (!launched.Spawned || !launched.Resumed || !restored
			|| !channel.has_value()) {
			return false;
		}
		const auto process   = launched.Process;
		auto       failChild = [&] {
			static_cast<void>(::kill(process, SIGKILL));
			int status = 0;
			while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
			}
			return false;
		};
		// fact_4. The handshake completes before the child reaches Qt or any
		// public observation, and it is what tells the child it was launched by
		// us. The identity is taken from the kernel audit trailer the child's
		// message carries, not from `process`: a pid can be reused between the
		// spawn and the question, and an audit token names the sender.
		if (!channel->authenticate(process, application)) {
			return failChild();
		}
		return wait_success(process);
	}

	[[nodiscard]] auto exact_arguments(
		const int aArgumentCount, char** aArguments, const std::string_view aMode)
		-> std::optional<std::filesystem::path>
	{
		if (aArgumentCount != 4 || aArguments == nullptr
			|| aArguments[0] == nullptr || aArguments[1] == nullptr
			|| aArguments[2] == nullptr || aArguments[3] == nullptr
			|| std::string_view{ aArguments[1] } != aMode
			|| std::string_view{ aArguments[2] } != "--inventory") {
			return std::nullopt;
		}
		auto inventory = std::filesystem::path{ aArguments[3] };
		if (!inventory.is_absolute() || inventory.lexically_normal() != inventory) {
			return std::nullopt;
		}
		return inventory;
	}

	struct attended_arguments
	{
		std::filesystem::path Inventory;
		std::filesystem::path Approval;
		std::string           Profile = "1";
	};

	[[nodiscard]] auto exact_attended_arguments(
		const int aArgumentCount, char** aArguments)
		-> std::optional<attended_arguments>
	{
		if ((aArgumentCount != 6 && aArgumentCount != 8) || aArguments == nullptr
			|| aArguments[0] == nullptr || aArguments[1] == nullptr
			|| aArguments[2] == nullptr || aArguments[3] == nullptr
			|| aArguments[4] == nullptr || aArguments[5] == nullptr
			|| std::string_view{ aArguments[1] } != "--attended-launch"
			|| std::string_view{ aArguments[2] } != "--inventory"
			|| std::string_view{ aArguments[4] } != "--approval") {
			return std::nullopt;
		}
		auto profile = std::string{ "1" };
		if (aArgumentCount == 8) {
			if (aArguments[6] == nullptr || aArguments[7] == nullptr
				|| std::string_view{ aArguments[6] } != "--bpr-profile") {
				return std::nullopt;
			}
			profile           = aArguments[7];
			auto       id     = std::uint64_t{ };
			const auto parsed = std::from_chars(profile.data(), profile.data() + profile.size(), id);
			if (parsed.ec != std::errc{ } || parsed.ptr != profile.data() + profile.size()
				|| id == 0U || std::to_string(id) != profile) {
				return std::nullopt;
			}
		}
		auto inventory = std::filesystem::path{ aArguments[3] };
		auto approval  = std::filesystem::path{ aArguments[5] };
		if (!inventory.is_absolute() || inventory.lexically_normal() != inventory
			|| !approval.is_absolute() || approval.lexically_normal() != approval) {
			return std::nullopt;
		}
		return attended_arguments{
			.Inventory = std::move(inventory), .Approval = std::move(approval), .Profile = std::move(profile)
		};
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	if (!lowercase_sha256(source_sha256) || !lowercase_sha256(verifier_sha256)) {
		return EXIT_FAILURE;
	}
	const auto verifyInventory = exact_arguments(argc, argv, "--verify-only");
	const auto probeInventory  = exact_arguments(argc, argv, "--probe-launch");
	const auto attended        = exact_attended_arguments(argc, argv);
	if (static_cast<unsigned>(verifyInventory.has_value())
			+ static_cast<unsigned>(probeInventory.has_value())
			+ static_cast<unsigned>(attended.has_value())
		!= 1U) {
		return EXIT_FAILURE;
	}
	const auto executable = running_executable();
	if (!executable.has_value() || !verify_running_self(*executable)) {
		return EXIT_FAILURE;
	}
	const auto root = capsule_root(*executable);
	if (!root.has_value()) {
		return EXIT_FAILURE;
	}
	const std::filesystem::path* inventory = nullptr;
	if (verifyInventory.has_value()) {
		inventory = &*verifyInventory;
	} else if (probeInventory.has_value()) {
		inventory = &*probeInventory;
	} else {
		inventory = &attended->Inventory;
	}
	const auto retainedSession
		= bpr::app::designated_live_supervisor_identity::session::open(
			*root, *inventory, verifier_sha256);
	if (!retainedSession.has_value()) {
		return EXIT_FAILURE;
	}
	if (!run_static_verifier(*retainedSession)) {
		return EXIT_FAILURE;
	}
	if (verifyInventory.has_value()) {
		return EXIT_SUCCESS;
	}
	if (probeInventory.has_value()) {
		return launch_harmless_probe(*retainedSession) ? EXIT_SUCCESS
													   : EXIT_FAILURE;
	}
	const auto observed
		= bpr::adapters::github::detail::observe_designated_live_public_target();
	if (!observed.has_value()) {
		return EXIT_FAILURE;
	}
	const auto launched = launch_attended_application(
		attended->Approval, attended->Profile, *retainedSession);
	return launched ? EXIT_SUCCESS : EXIT_FAILURE;
}
