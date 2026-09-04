#include "macos_keychain.hpp"
#include "macos_keychain_test_support.hpp"

#include "bpr/application/manual_executor.hpp"

#include <AvailabilityMacros.h>
#include <Security/SecRandom.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <span>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::tests::macos_keychain;

	inline constexpr std::size_t   native_canary_bytes        = 73U;
	inline constexpr std::uint64_t native_session             = 0x4250522200000001ULL;
	inline constexpr std::uint64_t native_generation          = 0x4250522200000002ULL;
	inline constexpr std::size_t   maximum_audited_file_bytes = 128U * 1024U * 1024U;

	[[nodiscard]] auto add_child_directory_action(
		posix_spawn_file_actions_t& aActions, const char* aPath) -> int
	{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
		if (__builtin_available(macOS 26.0, *)) {
			return posix_spawn_file_actions_addchdir(&aActions, aPath);
		}
#endif
		// Keep the fixture usable on macOS versions preceding the standard API.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		return posix_spawn_file_actions_addchdir_np(&aActions, aPath);
#pragma clang diagnostic pop
	}

	class secure_bytes final
	{
	public:
		explicit secure_bytes(const std::size_t aSize)
			: Bytes(aSize)
		{
		}

		~secure_bytes() noexcept
		{
			erase(Bytes);
		}

		secure_bytes(const secure_bytes&)                    = delete;
		auto operator=(const secure_bytes&) -> secure_bytes& = delete;
		secure_bytes(secure_bytes&&) noexcept                = default;
		auto operator=(secure_bytes&& aOther) noexcept -> secure_bytes&
		{
			if (this != &aOther) {
				erase(Bytes);
				Bytes = std::move(aOther.Bytes);
			}
			return *this;
		}

		[[nodiscard]] auto mutable_bytes() noexcept -> std::span<std::byte>
		{
			return Bytes;
		}

		[[nodiscard]] auto bytes() const noexcept
			-> std::span<const std::byte>
		{
			return Bytes;
		}

	private:
		std::vector<std::byte> Bytes;
	};

	class file_descriptor final
	{
	public:
		file_descriptor() = default;
		explicit file_descriptor(const int aDescriptor) noexcept
			: Descriptor{ aDescriptor }
		{
		}

		~file_descriptor() noexcept
		{
			reset();
		}

		file_descriptor(const file_descriptor&)                    = delete;
		auto operator=(const file_descriptor&) -> file_descriptor& = delete;

		file_descriptor(file_descriptor&& aOther) noexcept
			: Descriptor{ std::exchange(aOther.Descriptor, -1) }
		{
		}

		auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
		{
			if (this != &aOther) {
				reset();
				Descriptor = std::exchange(aOther.Descriptor, -1);
			}
			return *this;
		}

		[[nodiscard]] auto get() const noexcept -> int
		{
			return Descriptor;
		}

		[[nodiscard]] auto release() noexcept -> int
		{
			return std::exchange(Descriptor, -1);
		}

		void reset(const int aDescriptor = -1) noexcept
		{
			if (Descriptor >= 0) {
				static_cast<void>(::close(Descriptor));
			}
			Descriptor = aDescriptor;
		}

	private:
		int Descriptor = -1;
	};

	class audit_directory final
	{
	public:
		audit_directory() = default;
		~audit_directory() noexcept
		{
			reset();
		}

		audit_directory(const audit_directory&)                    = delete;
		auto operator=(const audit_directory&) -> audit_directory& = delete;

		[[nodiscard]] static auto create()
			-> std::optional<audit_directory>
		{
			std::array<char, 45U>      pattern{ };
			constexpr std::string_view prefix = "/tmp/betterpullreview-keychain-audit-XXXXXX";
			std::ranges::copy(prefix, pattern.begin());
			auto* created = ::mkdtemp(pattern.data());
			if (created == nullptr) {
				return std::nullopt;
			}

			audit_directory directory;
			directory.Path = created;
			std::error_code error;
			for (const auto& child : {
					 "home",
					 "tmp",
					 "config",
					 "data",
					 "cache",
				 }) {
				std::filesystem::create_directory(
					directory.Path / child,
					error);
				if (error) {
					return std::nullopt;
				}
			}
			return directory;
		}

		audit_directory(audit_directory&& aOther) noexcept
			: Path{ std::move(aOther.Path) }
		{
			aOther.Path.clear();
		}

		auto operator=(audit_directory&& aOther) noexcept
			-> audit_directory&
		{
			if (this != &aOther) {
				reset();
				Path = std::move(aOther.Path);
				aOther.Path.clear();
			}
			return *this;
		}

		[[nodiscard]] auto path() const noexcept
			-> const std::filesystem::path&
		{
			return Path;
		}

	private:
		void reset() noexcept
		{
			if (!Path.empty()) {
				std::error_code ignored;
				std::filesystem::remove_all(Path, ignored);
				Path.clear();
			}
		}

		std::filesystem::path Path;
	};

	template <typename Completion>
	struct terminal_recorder
	{
		std::size_t               Calls = 0U;
		std::optional<Completion> Terminal;

		void record(Completion aCompletion)
		{
			++Calls;
			Terminal.emplace(std::move(aCompletion));
		}
	};

	using store_recorder    = terminal_recorder<application::github_credential_store_completion>;
	using retrieve_recorder = terminal_recorder<application::github_credential_retrieve_completion>;
	using delete_recorder   = terminal_recorder<application::github_credential_delete_completion>;

	template <typename Terminal, typename Completion>
	[[nodiscard]] auto terminal_as(const std::optional<Completion>& aCompletion)
		-> const Terminal*
	{
		return aCompletion.has_value()
			? std::get_if<Terminal>(&*aCompletion)
			: nullptr;
	}

	[[nodiscard]] auto native_authority()
		-> application::github_credential_authority_key
	{
		return authority(
			0x4250522200000011ULL,
			0x4250522200000012ULL,
			0x4250522200000013ULL,
			0x4250522200000014ULL);
	}

	[[nodiscard]] auto native_context(const std::uint64_t aRequest)
		-> application::github_credential_operation_context
	{
		return context(
			native_authority(),
			aRequest,
			native_session,
			native_generation);
	}

	[[nodiscard]] auto run_store(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aBytes) -> bool
	{
		auto material = application::github_credential_material::create(aBytes);
		if (!material.has_value()) {
			return false;
		}

		const auto     operation = native_context(aRequest);
		store_recorder recorder;
		aVault.store(
			application::github_credential_enrollment{
				.Context    = operation,
				.Credential = std::move(*material),
			},
			[&recorder](
				application::github_credential_store_completion aTerminal) {
				recorder.record(std::move(aTerminal));
			});
		if (aLane.pending() != 1U) {
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_store_success>(recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& success->Disposition
			== application::github_credential_store_disposition::stored;
	}

	[[nodiscard]] auto run_retrieve(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::span<const std::byte>           aExpected) -> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		aVault.retrieve(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion aTerminal) {
				recorder.record(std::move(aTerminal));
			});
		if (aLane.pending() != 1U) {
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_retrieve_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& same_bytes(success->Credential.bytes(), aExpected);
	}

	[[nodiscard]] auto run_retrieve_absent(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest) -> bool
	{
		const auto        operation = native_context(aRequest);
		retrieve_recorder recorder;
		aVault.retrieve(
			operation,
			[&recorder](
				application::github_credential_retrieve_completion aTerminal) {
				recorder.record(std::move(aTerminal));
			});
		if (aLane.pending() != 1U) {
			return false;
		}
		aLane.run_next();

		const auto* absent = terminal_as<
			application::github_credential_retrieve_absent>(
			recorder.Terminal);
		return recorder.Calls == 1U && absent != nullptr
			&& absent->Context == operation;
	}

	[[nodiscard]] auto run_delete(
		application::github_credential_vault_port& aVault,
		application::manual_executor&              aLane,
		const std::uint64_t                        aRequest,
		const std::optional<
			application::github_credential_delete_disposition>
			aExpected) -> bool
	{
		const auto      operation = native_context(aRequest);
		delete_recorder recorder;
		aVault.delete_credential(
			operation,
			[&recorder](
				application::github_credential_delete_completion aTerminal) {
				recorder.record(std::move(aTerminal));
			});
		if (aLane.pending() != 1U) {
			return false;
		}
		aLane.run_next();

		const auto* success = terminal_as<
			application::github_credential_delete_success>(
			recorder.Terminal);
		return recorder.Calls == 1U && success != nullptr
			&& success->Context == operation
			&& (!aExpected.has_value()
				|| success->Disposition == *aExpected);
	}

	[[nodiscard]] auto make_native_vault(
		application::manual_executor& aLane)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
		auto vault = make_macos_keychain_integration_test_vault(aLane);
		return vault.has_value() ? std::move(*vault) : nullptr;
	}

	[[nodiscard]] auto ensure_native_item_absent(
		const std::uint64_t aRequestBase) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		const auto deletion = run_delete(*vault, lane, aRequestBase, std::nullopt);
		const auto absence  = run_retrieve_absent(*vault, lane, aRequestBase + 1U);
		return deletion && absence;
	}

	[[nodiscard]] auto run_native_lifecycle(
		const std::span<const std::byte> aCanaryA,
		const std::span<const std::byte> aCanaryB) -> bool
	{
		application::manual_executor lane;
		auto                         vault = make_native_vault(lane);
		if (vault == nullptr) {
			return false;
		}

		auto ok = true;
		ok      = run_delete(*vault, lane, 100U, std::nullopt) && ok;
		ok      = run_retrieve_absent(*vault, lane, 101U) && ok;
		ok      = run_store(*vault, lane, 102U, aCanaryA) && ok;
		ok      = run_retrieve(*vault, lane, 103U, aCanaryA) && ok;
		ok      = run_store(*vault, lane, 104U, aCanaryB) && ok;
		ok      = run_retrieve(*vault, lane, 105U, aCanaryB) && ok;
		ok      = run_delete(
					  *vault,
					  lane,
					  106U,
					  application::github_credential_delete_disposition::deleted)
			&& ok;
		ok = run_retrieve_absent(*vault, lane, 107U) && ok;
		ok = run_delete(
				 *vault,
				 lane,
				 108U,
				 application::github_credential_delete_disposition::absent)
			&& ok;

		const auto unconditionalCleanup = run_delete(*vault, lane, 109U, std::nullopt);
		const auto postCleanupAbsence   = run_retrieve_absent(*vault, lane, 110U);
		return ok && unconditionalCleanup && postCleanupAbsence;
	}

	[[nodiscard]] auto read_exact(
		const int                  aDescriptor,
		const std::span<std::byte> aOutput) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < aOutput.size()) {
			const auto count = ::read(
				aDescriptor,
				aOutput.data() + offset,
				aOutput.size() - offset);
			if (count > 0) {
				offset += static_cast<std::size_t>(count);
				continue;
			}
			if (count < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] auto write_exact(
		const int                        aDescriptor,
		const std::span<const std::byte> aInput) -> bool
	{
		auto offset = std::size_t{ 0U };
		while (offset < aInput.size()) {
			const auto count = ::write(
				aDescriptor,
				aInput.data() + offset,
				aInput.size() - offset);
			if (count > 0) {
				offset += static_cast<std::size_t>(count);
				continue;
			}
			if (count < 0 && errno == EINTR) {
				continue;
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] auto read_frame(const int aDescriptor)
		-> std::optional<secure_bytes>
	{
		std::array<std::byte, 4U> header{ };
		if (!read_exact(aDescriptor, header)) {
			return std::nullopt;
		}
		const auto length = (static_cast<std::uint32_t>(
								 std::to_integer<std::uint8_t>(header[0]))
							 << 24U)
			| (static_cast<std::uint32_t>(
				   std::to_integer<std::uint8_t>(header[1]))
			   << 16U)
			| (static_cast<std::uint32_t>(
				   std::to_integer<std::uint8_t>(header[2]))
			   << 8U)
			| static_cast<std::uint32_t>(
								std::to_integer<std::uint8_t>(header[3]));
		if (
			length == 0U
			|| length > application::maximum_github_credential_bytes) {
			return std::nullopt;
		}

		secure_bytes bytes{ length };
		if (!read_exact(aDescriptor, bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto write_frame(
		const int                        aDescriptor,
		const std::span<const std::byte> aBytes) -> bool
	{
		if (
			aBytes.empty()
			|| aBytes.size() > application::maximum_github_credential_bytes) {
			return false;
		}
		const auto       length = static_cast<std::uint32_t>(aBytes.size());
		const std::array header{
			static_cast<std::byte>((length >> 24U) & 0xFFU),
			static_cast<std::byte>((length >> 16U) & 0xFFU),
			static_cast<std::byte>((length >> 8U) & 0xFFU),
			static_cast<std::byte>(length & 0xFFU),
		};
		return write_exact(aDescriptor, header)
			&& write_exact(aDescriptor, aBytes);
	}

	[[nodiscard]] auto printable(
		const std::span<const std::byte> aBytes) -> bool
	{
		return std::ranges::all_of(aBytes, [](const std::byte aByte) {
			const auto value = std::to_integer<unsigned int>(aByte);
			return value >= 0x21U && value <= 0x7EU;
		});
	}

	[[nodiscard]] auto random_printable_bytes()
		-> std::optional<secure_bytes>
	{
		secure_bytes bytes{ native_canary_bytes };
		if (
			SecRandomCopyBytes(
				kSecRandomDefault,
				bytes.mutable_bytes().size(),
				bytes.mutable_bytes().data())
			!= errSecSuccess) {
			return std::nullopt;
		}
		for (auto& byte : bytes.mutable_bytes()) {
			const auto random = std::to_integer<unsigned int>(byte);
			byte              = static_cast<std::byte>(0x21U + (random % 94U));
		}
		return bytes;
	}

	[[nodiscard]] auto write_category(
		const std::string_view aCategory) -> bool
	{
		return write_exact(
			STDOUT_FILENO,
			std::as_bytes(std::span{ aCategory }));
	}

	[[nodiscard]] auto native_child() -> int
	{
		auto       canaryA = read_frame(STDIN_FILENO);
		auto       canaryB = read_frame(STDIN_FILENO);
		std::byte  extra{ };
		const auto trailing   = ::read(STDIN_FILENO, &extra, 1U);
		const auto inputValid = canaryA.has_value() && canaryB.has_value()
			&& !same_bytes(canaryA->bytes(), canaryB->bytes())
			&& printable(canaryA->bytes())
			&& printable(canaryB->bytes()) && trailing == 0;
		const auto passed = inputValid
			&& run_native_lifecycle(
								canaryA->bytes(),
								canaryB->bytes());
		static_cast<void>(write_category(
			passed ? "macos-keychain-native:pass\n"
				   : "macos-keychain-native:fail\n"));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	[[nodiscard]] auto string_contains(
		const std::string_view           aText,
		const std::span<const std::byte> aBytes) -> bool
	{
		if (aBytes.empty() || aText.size() < aBytes.size()) {
			return false;
		}
		return std::search(
				   aText.begin(),
				   aText.end(),
				   aBytes.begin(),
				   aBytes.end(),
				   [](const char aCharacter, const std::byte aByte) {
					   return static_cast<unsigned char>(aCharacter)
						   == std::to_integer<unsigned char>(aByte);
				   })
			!= aText.end();
	}

	[[nodiscard]] auto read_file(
		const std::filesystem::path& aPath)
		-> std::optional<secure_bytes>
	{
		std::error_code error;
		const auto      size = std::filesystem::file_size(aPath, error);
		if (error || size > maximum_audited_file_bytes) {
			return std::nullopt;
		}

		file_descriptor input{
			::open(aPath.c_str(), O_RDONLY | O_CLOEXEC)
		};
		if (input.get() < 0) {
			return std::nullopt;
		}
		secure_bytes bytes{ static_cast<std::size_t>(size) };
		if (!read_exact(input.get(), bytes.mutable_bytes())) {
			return std::nullopt;
		}
		return bytes;
	}

	[[nodiscard]] auto file_contains(
		const std::filesystem::path&     aPath,
		const std::span<const std::byte> aNeedle)
		-> std::optional<bool>
	{
		auto bytes = read_file(aPath);
		if (!bytes.has_value()) {
			return std::nullopt;
		}
		return std::ranges::search(bytes->bytes(), aNeedle).begin()
			!= bytes->bytes().end();
	}

	[[nodiscard]] auto exact_file_text(
		const std::filesystem::path& aPath,
		const std::string_view       aExpected) -> bool
	{
		auto bytes = read_file(aPath);
		return bytes.has_value()
			&& bytes->bytes().size() == aExpected.size()
			&& std::ranges::equal(
				   bytes->bytes(),
				   std::as_bytes(std::span{ aExpected }));
	}

	[[nodiscard]] auto canaries_absent_from_file(
		const std::filesystem::path&     aPath,
		const std::span<const std::byte> aCanaryA,
		const std::span<const std::byte> aCanaryB) -> bool
	{
		const auto containsA = file_contains(aPath, aCanaryA);
		const auto containsB = file_contains(aPath, aCanaryB);
		return containsA.has_value() && containsB.has_value()
			&& !*containsA && !*containsB;
	}

	[[nodiscard]] auto canaries_absent_from_tree(
		const std::filesystem::path&     aRoot,
		const std::span<const std::byte> aCanaryA,
		const std::span<const std::byte> aCanaryB) -> bool
	{
		std::error_code error;
		auto            iterator = std::filesystem::recursive_directory_iterator{
			aRoot,
			std::filesystem::directory_options::skip_permission_denied,
			error
		};
		const auto end = std::filesystem::recursive_directory_iterator{ };
		if (error) {
			return false;
		}
		while (iterator != end) {
			const auto entry = *iterator;
			iterator.increment(error);
			if (error) {
				return false;
			}
			if (entry.is_regular_file(error)) {
				if (
					error
					|| !canaries_absent_from_file(
						entry.path(),
						aCanaryA,
						aCanaryB)) {
					return false;
				}
			} else if (error) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto make_child_environment(
		const std::filesystem::path& aRoot) -> std::vector<std::string>
	{
		const auto* home = std::getenv("HOME");
		if (home == nullptr || *home == '\0') {
			return { };
		}
		return {
			"HOME=" + std::string{ home },
			"TMPDIR=" + (aRoot / "tmp").string(),
			"XDG_CONFIG_HOME=" + (aRoot / "config").string(),
			"XDG_DATA_HOME=" + (aRoot / "data").string(),
			"XDG_CACHE_HOME=" + (aRoot / "cache").string(),
			"PATH=/usr/bin:/bin",
			"LANG=C",
		};
	}

	[[nodiscard]] auto audit_native_child(
		const int   aParentArgumentCount,
		char* const aParentArguments[]) -> bool
	{
		auto canaryA = random_printable_bytes();
		auto canaryB = random_printable_bytes();
		if (!canaryA.has_value() || !canaryB.has_value()) {
			return false;
		}
		while (same_bytes(canaryA->bytes(), canaryB->bytes())) {
			canaryB = random_printable_bytes();
			if (!canaryB.has_value()) {
				return false;
			}
		}

		auto directory = audit_directory::create();
		if (!directory.has_value()) {
			return false;
		}

		std::error_code pathError;
		const auto      executable = std::filesystem::canonical(aParentArguments[0], pathError);
		if (pathError) {
			return false;
		}
		const auto          stdoutPath = directory->path() / "stdout.audit";
		const auto          stderrPath = directory->path() / "stderr.audit";
		file_descriptor     stdoutFile{ ::open(
			stdoutPath.c_str(),
			O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
			S_IRUSR | S_IWUSR) };
		file_descriptor     stderrFile{ ::open(
			stderrPath.c_str(),
			O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
			S_IRUSR | S_IWUSR) };
		std::array<int, 2U> inputPipe{ -1, -1 };
		if (
			stdoutFile.get() < 0 || stderrFile.get() < 0
			|| ::pipe(inputPipe.data()) != 0) {
			if (inputPipe[0] >= 0) {
				static_cast<void>(::close(inputPipe[0]));
			}
			if (inputPipe[1] >= 0) {
				static_cast<void>(::close(inputPipe[1]));
			}
			return false;
		}
		file_descriptor childInput{ inputPipe[0] };
		file_descriptor parentInput{ inputPipe[1] };
		if (::fcntl(parentInput.get(), F_SETNOSIGPIPE, 1) != 0) {
			return false;
		}

		posix_spawn_file_actions_t actions;
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return false;
		}
		auto actionsValid = true;
		actionsValid      = posix_spawn_file_actions_adddup2(
								&actions,
								childInput.get(),
								STDIN_FILENO)
				== 0
			&& actionsValid;
		actionsValid = posix_spawn_file_actions_adddup2(
						   &actions,
						   stdoutFile.get(),
						   STDOUT_FILENO)
				== 0
			&& actionsValid;
		actionsValid = posix_spawn_file_actions_adddup2(
						   &actions,
						   stderrFile.get(),
						   STDERR_FILENO)
				== 0
			&& actionsValid;
		actionsValid = posix_spawn_file_actions_addclose(
						   &actions,
						   parentInput.get())
				== 0
			&& actionsValid;
		actionsValid = add_child_directory_action(
						   actions,
						   directory->path().c_str())
				== 0
			&& actionsValid;

		auto environment = make_child_environment(directory->path());
		if (environment.empty()) {
			static_cast<void>(
				posix_spawn_file_actions_destroy(&actions));
			return false;
		}
		std::vector<char*> environmentPointers;
		environmentPointers.reserve(environment.size() + 1U);
		for (auto& value : environment) {
			environmentPointers.push_back(value.data());
		}
		environmentPointers.push_back(nullptr);

		auto                  executableText = executable.string();
		auto                  childMode      = std::string{ "--native-child" };
		std::array<char*, 3U> childArguments{
			executableText.data(),
			childMode.data(),
			nullptr,
		};

		auto auditOk = actionsValid;
		for (auto index = 0; index < aParentArgumentCount; ++index) {
			const auto argument = std::string_view{ aParentArguments[index] };
			auditOk             = !string_contains(argument, canaryA->bytes())
				&& !string_contains(argument, canaryB->bytes())
				&& auditOk;
		}
		for (const auto* argument : childArguments) {
			if (argument != nullptr) {
				const auto value = std::string_view{ argument };
				auditOk          = !string_contains(value, canaryA->bytes())
					&& !string_contains(value, canaryB->bytes())
					&& auditOk;
			}
		}
		for (const auto& value : environment) {
			auditOk = !string_contains(value, canaryA->bytes())
				&& !string_contains(value, canaryB->bytes())
				&& auditOk;
		}

		pid_t      child       = -1;
		const auto spawnStatus = actionsValid
			? posix_spawn(
				  &child,
				  executableText.c_str(),
				  &actions,
				  nullptr,
				  childArguments.data(),
				  environmentPointers.data())
			: EINVAL;
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));
		childInput.reset();
		stdoutFile.reset();
		stderrFile.reset();
		if (spawnStatus != 0) {
			return false;
		}

		const auto wrote = write_frame(parentInput.get(), canaryA->bytes())
			&& write_frame(parentInput.get(), canaryB->bytes());
		parentInput.reset();

		int        childStatus = 0;
		auto       waited      = false;
		const auto deadline    = std::chrono::steady_clock::now()
			+ std::chrono::seconds{ 15 };
		while (std::chrono::steady_clock::now() < deadline) {
			const auto result = ::waitpid(child, &childStatus, WNOHANG);
			if (result == child) {
				waited = true;
				break;
			}
			if (result < 0 && errno == EINTR) {
				continue;
			}
			if (result < 0) {
				break;
			}
			static_cast<void>(::usleep(10'000U));
		}
		if (!waited) {
			static_cast<void>(::kill(child, SIGKILL));
			while (::waitpid(child, &childStatus, 0) < 0 && errno == EINTR) {
			}
		}
		const auto parentCleanup = ensure_native_item_absent(3'000U);

		auditOk = waited && wrote && WIFEXITED(childStatus)
			&& WEXITSTATUS(childStatus) == EXIT_SUCCESS && auditOk;
		auditOk = exact_file_text(
					  stdoutPath,
					  "macos-keychain-native:pass\n")
			&& exact_file_text(stderrPath, "") && auditOk;
		auditOk = canaries_absent_from_file(
					  executable,
					  canaryA->bytes(),
					  canaryB->bytes())
			&& canaries_absent_from_file(
					  __FILE__,
					  canaryA->bytes(),
					  canaryB->bytes())
			&& canaries_absent_from_tree(
					  directory->path(),
					  canaryA->bytes(),
					  canaryB->bytes())
			&& parentCleanup && auditOk;
		return auditOk;
	}

	[[nodiscard]] auto fixture_mode(
		const bool aSetup) -> int
	{
		const auto requestBase = aSetup ? 1'000U : 2'000U;
		const auto passed      = ensure_native_item_absent(requestBase);
		static_cast<void>(write_category(
			aSetup
				? (passed ? "macos-keychain-setup:pass\n"
						  : "macos-keychain-setup:fail\n")
				: (passed ? "macos-keychain-cleanup:pass\n"
						  : "macos-keychain-cleanup:fail\n")));
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}

auto main(const int argument_count, char* arguments[]) -> int
{
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--native-child") {
		return native_child();
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--fixture-setup") {
		return fixture_mode(true);
	}
	if (
		argument_count == 2
		&& std::string_view{ arguments[1] } == "--fixture-cleanup") {
		return fixture_mode(false);
	}
	if (argument_count != 1) {
		static_cast<void>(
			write_category("macos-keychain-audit:fail\n"));
		return EXIT_FAILURE;
	}

	const auto passed = audit_native_child(argument_count, arguments);
	static_cast<void>(write_category(
		passed ? "macos-keychain-audit:pass\n"
			   : "macos-keychain-audit:fail\n"));
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
