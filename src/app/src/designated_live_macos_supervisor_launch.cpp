#include "designated_live_macos_supervisor_launch.hpp"

#if defined(__APPLE__)

	#include <cerrno>
	#include <csignal>
	#include <sys/wait.h>
	#include <unistd.h>

	#include <array>
	#include <string>
	#include <utility>

namespace bpr::app::designated_live_supervisor_launch {
	namespace {
		[[nodiscard]] auto spawn_with_actions(
			pid_t*                            aProcess,
			const char*                       aExecutable,
			const posix_spawn_file_actions_t* aActions,
			const posix_spawnattr_t*          aAttributes,
			char* const                       aArguments[],
			char* const                       aEnvironment[]) -> int
		{
			return ::posix_spawn(
				aProcess,
				aExecutable,
				aActions,
				aAttributes,
				aArguments,
				aEnvironment);
		}

		void terminate_and_wait(const pid_t aProcess) noexcept
		{
			static_cast<void>(::kill(aProcess, SIGKILL));
			int status = 0;
			while (::waitpid(aProcess, &status, 0) < 0 && errno == EINTR) {
			}
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
	} // namespace

	auto operations::production(std::function<bool(pid_t)> aPostSpawnCheck)
		-> operations
	{
		return operations{
			.Barrier        = { },
			.Spawn          = spawn_with_actions,
			.PostSpawnCheck = std::move(aPostSpawnCheck),
			.Resume         = [](const pid_t aProcess) { return ::kill(aProcess, SIGCONT); },
			.Refuse = terminate_and_wait,
		};
	}

	auto launch(
		const designated_live_supervisor_identity::session& aSession,
		const std::filesystem::path&                        aApplication,
		const posix_spawn_file_actions_t&                   aActions,
		const posix_spawnattr_t&                            aAttributes,
		char* const                                         aArguments[],
		char* const                                         aEnvironment[],
		operations                                          aOperations) -> outcome
	{
		auto       result           = outcome{ };
		const auto fixedApplication = std::filesystem::path{ application_relative };
		if (aApplication != fixedApplication || aApplication.is_absolute()
			|| aApplication.lexically_normal() != aApplication
			|| !aOperations.Spawn || !aOperations.PostSpawnCheck
			|| !aOperations.Resume || !aOperations.Refuse) {
			return result;
		}
		if (aOperations.Barrier) {
			aOperations.Barrier(phase::BeforeSpawn);
		}
		if (!aSession.revalidate()) {
			return result;
		}
		if (aOperations.Spawn(
				&result.Process,
				fixedApplication.c_str(),
				&aActions,
				&aAttributes,
				aArguments,
				aEnvironment)
			!= 0) {
			result.Process = -1;
			return result;
		}
		result.Spawned = true;
		if (!aOperations.PostSpawnCheck(result.Process)) {
			aOperations.Refuse(result.Process);
			return result;
		}
		if (aOperations.Barrier) {
			aOperations.Barrier(phase::BeforeResume);
		}
		if (!aSession.revalidate() || aOperations.Resume(result.Process) != 0) {
			aOperations.Refuse(result.Process);
			return result;
		}
		result.Resumed = true;
		return result;
	}

	auto verifier_operations::production() -> verifier_operations
	{
		return verifier_operations{
			.ConfigureAttributes = [](posix_spawnattr_t& aAttributes) {
				return posix_spawnattr_setflags(
						   &aAttributes,
						   static_cast<short>(POSIX_SPAWN_CLOEXEC_DEFAULT))
					== 0;
			},
			.Spawn = spawn_with_actions,
			.Wait  = wait_success,
		};
	}

	auto verify_static(
		const designated_live_supervisor_identity::session& aSession,
		const std::string_view                              aSourceSha256,
		verifier_operations                                 aOperations)
		-> verifier_outcome
	{
		auto result = verifier_outcome{ };
		if (aSourceSha256.size() != 64U || !aOperations.ConfigureAttributes
			|| !aOperations.Spawn || !aOperations.Wait) {
			return result;
		}
		posix_spawn_file_actions_t actions{ };
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return result;
		}
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			return result;
		}
		auto transport = aSession.configure_verifier_spawn(actions);
		auto python    = std::string{ python_executable };
		auto isolated  = std::string{ "-I" };
		auto noSite    = std::string{ "-S" };
		auto verifier  = "/dev/fd/"
			+ std::to_string(
							designated_live_supervisor_identity::session::verifier_child_descriptor());
		auto verify        = std::string{ "verify" };
		auto capsuleFlag   = std::string{ "--capsule" };
		auto capsule       = std::string{ "." };
		auto inventoryFlag = std::string{ "--inventory-fd" };
		auto inventory     = std::to_string(
			designated_live_supervisor_identity::session::inventory_child_descriptor());
		auto sourceFlag  = std::string{ "--source-sha256" };
		auto source      = std::string{ aSourceSha256 };
		auto privateFlag = std::string{ "--require-owner-private" };
		auto arguments   = std::array<char*, 13>{
			python.data(),
			isolated.data(),
			noSite.data(),
			verifier.data(),
			verify.data(),
			capsuleFlag.data(),
			capsule.data(),
			inventoryFlag.data(),
			inventory.data(),
			sourceFlag.data(),
			source.data(),
			privateFlag.data(),
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
		const auto configured = transport.has_value()
			&& aOperations.ConfigureAttributes(attributes) && aSession.revalidate();
		const auto spawned = configured
			? aOperations.Spawn(
				  &result.Process,
				  python.c_str(),
				  &actions,
				  &attributes,
				  arguments.data(),
				  environment.data())
			: EINVAL;
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		if (spawned != 0) {
			result.Process = -1;
			return result;
		}
		result.Spawned = true;
		result.Transmitted
			= aSession.transmit_verifier_and_inventory(*transport);
		result.Succeeded   = aOperations.Wait(result.Process);
		result.Revalidated = aSession.revalidate();
		return result;
	}
}

#endif // defined(__APPLE__)
