#ifndef BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_HPP
#define BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_HPP

/// \file
/// \brief Pre-resume application launch controller for the macOS supervisor.

#if defined(__APPLE__)

	#include "designated_live_macos_supervisor_identity.hpp"

	#include <spawn.h>
	#include <sys/types.h>

	#include <filesystem>
	#include <functional>
	#include <string_view>

namespace bpr::app::designated_live_supervisor_launch {
	inline constexpr auto application_relative = std::string_view{
		"BetterPullReview.app/Contents/MacOS/BetterPullReview"
	};
	inline constexpr auto python_executable = std::string_view{ "/usr/bin/python3" };

	using spawn_operation = std::function<int(
		pid_t*,
		const char*,
		const posix_spawn_file_actions_t*,
		const posix_spawnattr_t*,
		char* const[],
		char* const[])>;

	enum class phase {
		BeforeSpawn,
		BeforeResume,
	};

	struct operations
	{
		std::function<void(phase)> Barrier;
		spawn_operation            Spawn;
		std::function<bool(pid_t)> PostSpawnCheck;
		std::function<int(pid_t)>  Resume;
		std::function<void(pid_t)> Refuse;

		[[nodiscard]] static auto production(
			std::function<bool(pid_t)> aPostSpawnCheck) -> operations;
	};

	struct outcome
	{
		pid_t Process = -1;
		bool  Spawned = false;
		bool  Resumed = false;
	};

	/// \brief Spawns only the fixed retained-root application and resumes it only
	/// after the caller's provenance checks and one final identity revalidation.
	[[nodiscard]] auto launch(
		const designated_live_supervisor_identity::session& aSession,
		const std::filesystem::path&                        aApplication,
		const posix_spawn_file_actions_t&                   aActions,
		const posix_spawnattr_t&                            aAttributes,
		char* const                                         aArguments[],
		char* const                                         aEnvironment[],
		operations                                          aOperations) -> outcome;

	struct verifier_operations
	{
		std::function<bool(posix_spawnattr_t&)> ConfigureAttributes;
		spawn_operation                         Spawn;
		std::function<bool(pid_t)>              Wait;

		[[nodiscard]] static auto production() -> verifier_operations;
	};

	struct verifier_outcome
	{
		pid_t Process     = -1;
		bool  Spawned     = false;
		bool  Transmitted = false;
		bool  Succeeded   = false;
		bool  Revalidated = false;

		[[nodiscard]] auto accepted() const noexcept -> bool
		{
			return Spawned && Transmitted && Succeeded && Revalidated;
		}
	};

	/// \brief Runs the digest-bound verifier through the retained root and
	/// anonymous source/inventory streams owned by the identity session.
	[[nodiscard]] auto verify_static(
		const designated_live_supervisor_identity::session& aSession,
		std::string_view                                    aSourceSha256,
		verifier_operations                                 aOperations)
		-> verifier_outcome;
}

#endif // defined(__APPLE__)

#endif // BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_HPP
