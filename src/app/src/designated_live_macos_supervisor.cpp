#include "designated_live_public_observer.hpp"

#include "designated_live_macos_capsule_identity.hpp"
#include "designated_live_macos_provenance.hpp"
#include <CoreFoundation/CoreFoundation.h>

#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>
#include <mach-o/dyld.h>

#include <array>
#include <cerrno>
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
		"BetterPullReview.app/Contents/Helpers/"
		"better_pull_review_designated_live_supervisor"
	};
	constexpr auto application_relative = std::string_view{
		"BetterPullReview.app/Contents/MacOS/better_pull_review"
	};
	constexpr auto verifier_relative = std::string_view{
		"BetterPullReview.app/Contents/Resources/"
		"designated_live_macos_capsule.py"
	};
	constexpr auto source_sha256            = std::string_view{ BPR_ATTENDED_REVIEW_SOURCE_SHA256 };
	constexpr auto child_channel_descriptor = 3;
	constexpr auto child_probe_argument     = std::string_view{
		"--designated-live-preflight-probe"
	};
	constexpr auto child_probe_acknowledgement = std::string_view{
		"BPR_DESIGNATED_LIVE_PREFLIGHT_OK\n"
	};
	constexpr auto maximum_inventory_bytes = std::size_t{ 16U * 1024U * 1024U };

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

	[[nodiscard]] auto read_file(
		const std::filesystem::path& aPath,
		const std::size_t            aLimit) -> std::optional<std::vector<std::uint8_t>>
	{
		auto file = file_descriptor{
			::open(aPath.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
		};
		struct stat status{ };
		if (!file || ::fstat(file.get(), &status) != 0 || !S_ISREG(status.st_mode)
			|| status.st_size < 0
			|| static_cast<std::uintmax_t>(status.st_size) > aLimit) {
			return std::nullopt;
		}
		auto bytes = std::vector<std::uint8_t>{ };
		bytes.reserve(static_cast<std::size_t>(status.st_size));
		auto buffer = std::array<std::uint8_t, 64U * 1024U>{ };
		for (;;) {
			const auto count = ::read(file.get(), buffer.data(), buffer.size());
			if (count < 0 && errno == EINTR) {
				continue;
			}
			if (count < 0) {
				return std::nullopt;
			}
			if (count == 0) {
				break;
			}
			const auto size = static_cast<std::size_t>(count);
			if (size > aLimit - bytes.size()) {
				return std::nullopt;
			}
			bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
		}
		return bytes;
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

	[[nodiscard]] auto spawn_checked(
		const std::filesystem::path& aExecutable,
		std::vector<std::string>     aArguments) -> bool
	{
		auto pointers = std::vector<char*>{ };
		pointers.reserve(aArguments.size() + 2U);
		auto executable = aExecutable.string();
		pointers.push_back(executable.data());
		for (auto& argument : aArguments) {
			pointers.push_back(argument.data());
		}
		pointers.push_back(nullptr);
		auto pathValue   = std::array{ 'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/',
									   'b', 'i', 'n', ':', '/', 'b', 'i', 'n', ':', '/', 'u', 's', 'r', '/', 's',
									   'b', 'i', 'n', ':', '/', 's', 'b', 'i', 'n', '\0' };
		auto langValue   = std::array{ 'L', 'A', 'N', 'G', '=', 'C', '\0' };
		auto localeValue = std::array{ 'L', 'C', '_', 'A', 'L', 'L', '=', 'C', '\0' };
		auto environment = std::array<char*, 4>{
			pathValue.data(), langValue.data(), localeValue.data(), nullptr
		};
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			return false;
		}
		auto       flags      = static_cast<short>(POSIX_SPAWN_CLOEXEC_DEFAULT);
		const auto configured = posix_spawnattr_setflags(&attributes, flags) == 0;
		pid_t      process    = -1;
		const auto spawned    = configured
			? posix_spawn(
				  &process,
				  executable.c_str(),
				  nullptr,
				  &attributes,
				  pointers.data(),
				  environment.data())
			: EINVAL;
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		return spawned == 0 && wait_success(process);
	}

	[[nodiscard]] auto run_static_verifier(
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aInventory) -> bool
	{
		return spawn_checked(
			"/usr/bin/python3",
			{
				"-I",
				"-S",
				(aRoot / verifier_relative).string(),
				"verify",
				"--capsule",
				aRoot.string(),
				"--inventory",
				aInventory.string(),
				"--source-sha256",
				std::string{ source_sha256 },
				"--require-owner-private",
			});
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
		const std::filesystem::path& aRoot) -> bool
	{
		const auto  application = aRoot / application_relative;
		auto        parent      = file_descriptor{ ::open(
			application.parent_path().c_str(),
			O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
		auto        executable  = file_descriptor{ ::openat(
			parent.get(),
			application.filename().c_str(),
			O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
		struct stat identity{ };
		if (!parent || !executable || ::fstat(executable.get(), &identity) != 0
			|| !S_ISREG(identity.st_mode) || identity.st_uid != ::geteuid()
			|| (identity.st_mode & 0022) != 0) {
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
		posix_spawnattr_t          attributes{ };
		if (posix_spawn_file_actions_init(&actions) != 0
			|| posix_spawnattr_init(&attributes) != 0) {
			return false;
		}
		auto cleanActions = [&] {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			static_cast<void>(posix_spawnattr_destroy(&attributes));
		};
		if (posix_spawn_file_actions_adddup2(
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
		auto executableArgument = application.string();
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
		pid_t      process = -1;
		const auto spawned = posix_spawn(
			&process,
			application.c_str(),
			&actions,
			&attributes,
			arguments.data(),
			environment.data());
		cleanActions();
		childSocket = file_descriptor{ };
		if (spawned != 0) {
			return false;
		}
		auto failChild = [&] {
			static_cast<void>(::kill(process, SIGKILL));
			int status = 0;
			while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
			}
			return false;
		};
		if (!exact_file_identity(
				parent.get(), application.filename().string(), identity)) {
			return failChild();
		}
		const auto childSigningDigest = guest_signing_digest(process);
		if (!childSigningDigest.has_value()
			|| *childSigningDigest != *expectedSigningDigest) {
			return failChild();
		}
		const auto challenge = std::string{ source_sha256 } + "\n";
		if (::write(parentSocket.get(), challenge.data(), challenge.size())
				!= static_cast<ssize_t>(challenge.size())
			|| ::kill(process, SIGCONT) != 0) {
			return failChild();
		}
		auto descriptor = pollfd{
			.fd = parentSocket.get(), .events = POLLIN, .revents = 0
		};
		if (::poll(&descriptor, 1, 10'000) <= 0
			|| (descriptor.revents & POLLIN) == 0) {
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

	/// \brief The SHA-256 of a file, as lowercase hex.
	///
	/// \details BPR-0262 fact_5 binds the inventory into the approval subject.
	/// The inventory describes the capsule and so cannot live inside it, which
	/// means the application cannot compute this for itself and stage one has
	/// to hand it over. Handing over the wrong one renders a subject the
	/// operator never approved, so the failure direction is refusal.
	[[nodiscard]] auto file_sha256_hex(const std::filesystem::path& aPath)
		-> std::optional<std::string>
	{
		const auto bytes = read_file(aPath, maximum_inventory_bytes);
		if (!bytes.has_value()) {
			return std::nullopt;
		}
		auto digest = std::array<unsigned char, CC_SHA256_DIGEST_LENGTH>{ };
		CC_SHA256(
			bytes->data(),
			static_cast<CC_LONG>(bytes->size()),
			digest.data());
		static constexpr auto hex  = std::string_view{ "0123456789abcdef" };
		auto                  text = std::string{ };
		text.reserve(digest.size() * 2U);
		for (const auto byte : digest) {
			text.push_back(hex[(byte >> 4U) & 0x0fU]);
			text.push_back(hex[byte & 0x0fU]);
		}
		return text;
	}

	[[nodiscard]] auto launch_attended_application(
		const std::filesystem::path& aRoot,
		const std::filesystem::path& aApproval,
		const std::string_view       aInventoryDigest,
		const int                    aRetainedRoot,
		const std::vector<
			bpr::app::designated_live_capsule_identity::entry>& aCapturedCapsule)
		-> bool
	{
		if (!aApproval.is_absolute() || aApproval.lexically_normal() != aApproval
			|| aInventoryDigest.size() != 64U) {
			return false;
		}
		const auto  application = aRoot / application_relative;
		auto        parent      = file_descriptor{ ::open(
			application.parent_path().c_str(),
			O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW) };
		auto        executable  = file_descriptor{ ::openat(
			parent.get(),
			application.filename().c_str(),
			O_RDONLY | O_CLOEXEC | O_NOFOLLOW) };
		struct stat identity{ };
		if (!parent || !executable || ::fstat(executable.get(), &identity) != 0
			|| !S_ISREG(identity.st_mode) || identity.st_uid != ::geteuid()
			|| (identity.st_mode & 0022) != 0) {
			return false;
		}
		const auto expectedSigningDigest = static_signing_digest(application);
		if (!expectedSigningDigest.has_value()) {
			return false;
		}
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
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
			static_cast<void>(posix_spawnattr_destroy(&attributes));
			return false;
		}
		auto executableArgument = application.string();
		auto approvalFlag       = std::string{ "--attended-designated-live-approval" };
		auto approvalArgument   = aApproval.string();
		auto inventoryFlag      = std::string{
			"--attended-designated-live-inventory-sha256"
		};
		auto inventoryArgument = std::string{ aInventoryDigest };
		auto arguments         = std::array<char*, 6>{
			executableArgument.data(),
			approvalFlag.data(),
			approvalArgument.data(),
			inventoryFlag.data(),
			inventoryArgument.data(),
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
		// fact_3. The right the child will authenticate us by is published
		// before the spawn, because inheritance is the only channel permitted:
		// there is no argument, variable, or descriptor here that could carry
		// it afterwards. An occupied slot refuses outright rather than being
		// overwritten.
		auto channel = bpr::app::designated_live_provenance::supervisor_channel::claim();
		if (!channel.has_value()) {
			static_cast<void>(posix_spawnattr_destroy(&attributes));
			return false;
		}
		pid_t      process = -1;
		const auto spawned = posix_spawn(
			&process,
			application.c_str(),
			nullptr,
			&attributes,
			arguments.data(),
			environment.data());
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		// Immediately, and whether or not the spawn worked. Every later spawn
		// from this task must find the slot as it was.
		const auto restored = channel->restore();
		if (spawned != 0 || !restored) {
			return false;
		}
		auto failChild = [&] {
			static_cast<void>(::kill(process, SIGKILL));
			int status = 0;
			while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
			}
			return false;
		};
		if (!exact_file_identity(
				parent.get(), application.filename().string(), identity)) {
			return failChild();
		}
		const auto childSigningDigest = guest_signing_digest(process);
		if (!childSigningDigest.has_value()
			|| *childSigningDigest != *expectedSigningDigest) {
			return failChild();
		}
		// fact_2, and the last moment it can be asked. The Python verifier that
		// walked this tree exited before the child was even spawned, and the
		// checks above cover only the top-level executable -- a nested dylib,
		// framework, plugin, or qt.conf could have been replaced in between and
		// nothing would have looked. Asked through the descriptor retained
		// before the verifier ran, so a swapped parent directory cannot answer
		// in place of the one that was verified.
		if (!bpr::app::designated_live_capsule_identity::unchanged(
				aRetainedRoot, aCapturedCapsule)) {
			return failChild();
		}
		if (::kill(process, SIGCONT) != 0) {
			return failChild();
		}
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
		const int argc, char** argv, const std::string_view aMode)
		-> std::optional<std::filesystem::path>
	{
		if (argc != 4 || argv == nullptr || argv[0] == nullptr || argv[1] == nullptr
			|| argv[2] == nullptr || argv[3] == nullptr
			|| std::string_view{ argv[1] } != aMode
			|| std::string_view{ argv[2] } != "--inventory") {
			return std::nullopt;
		}
		auto inventory = std::filesystem::path{ argv[3] };
		if (!inventory.is_absolute() || inventory.lexically_normal() != inventory) {
			return std::nullopt;
		}
		return inventory;
	}

	struct attended_arguments
	{
		std::filesystem::path Inventory;
		std::filesystem::path Approval;
	};

	[[nodiscard]] auto exact_attended_arguments(const int argc, char** argv)
		-> std::optional<attended_arguments>
	{
		if (argc != 6 || argv == nullptr || argv[0] == nullptr
			|| argv[1] == nullptr || argv[2] == nullptr || argv[3] == nullptr
			|| argv[4] == nullptr || argv[5] == nullptr
			|| std::string_view{ argv[1] } != "--attended-launch"
			|| std::string_view{ argv[2] } != "--inventory"
			|| std::string_view{ argv[4] } != "--approval") {
			return std::nullopt;
		}
		auto inventory = std::filesystem::path{ argv[3] };
		auto approval  = std::filesystem::path{ argv[5] };
		if (!inventory.is_absolute() || inventory.lexically_normal() != inventory
			|| !approval.is_absolute() || approval.lexically_normal() != approval) {
			return std::nullopt;
		}
		return attended_arguments{
			.Inventory = std::move(inventory), .Approval = std::move(approval)
		};
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	if (!lowercase_sha256(source_sha256)) {
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
	const auto inventoryBytes = read_file(*inventory, maximum_inventory_bytes);
	if (!inventoryBytes.has_value() || inventoryBytes->empty()) {
		return EXIT_FAILURE;
	}
	if (!run_static_verifier(*root, *inventory)) {
		return EXIT_FAILURE;
	}
	if (verifyInventory.has_value()) {
		return EXIT_SUCCESS;
	}
	if (probeInventory.has_value()) {
		return launch_harmless_probe(*root) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	const auto observed
		= bpr::adapters::github::detail::observe_designated_live_public_target();
	if (!observed.has_value()) {
		return EXIT_FAILURE;
	}
	const auto inventoryDigest = file_sha256_hex(attended->Inventory);
	if (!inventoryDigest.has_value()) {
		return EXIT_FAILURE;
	}
	// Retained across the whole window, and opened O_NOFOLLOW so the capsule
	// this describes is the capsule the verifier was pointed at.
	const auto retainedRoot
		= bpr::app::designated_live_capsule_identity::open_root(*root);
	if (!retainedRoot.has_value()) {
		return EXIT_FAILURE;
	}
	const auto capturedCapsule
		= bpr::app::designated_live_capsule_identity::capture(*retainedRoot);
	if (!capturedCapsule.has_value()) {
		static_cast<void>(::close(*retainedRoot));
		return EXIT_FAILURE;
	}
	const auto launched = launch_attended_application(
		*root, attended->Approval, *inventoryDigest, *retainedRoot,
		*capturedCapsule);
	static_cast<void>(::close(*retainedRoot));
	return launched ? EXIT_SUCCESS : EXIT_FAILURE;
}
