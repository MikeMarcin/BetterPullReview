#include "bpr/adapters/process_support/process_supervision.hpp"

#include <QObject>

#include <algorithm>
#include <limits>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <sys/types.h>
#endif

#include <memory>
#if defined(Q_OS_MACOS)
	#include <libproc.h>
	#include <time.h>
	#include <unistd.h>
#elif defined(Q_OS_WIN)
	#include <qt_windows.h>
#else
	#include <time.h>
#endif
namespace bpr::adapters::process_support {
	auto wait_milliseconds(const supervision_clock::time_point aDeadline) noexcept -> int
	{
		const auto remaining = aDeadline - supervision_clock::now();
		if (remaining <= supervision_clock::duration::zero()) {
			return 0;
		}

		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
		if (milliseconds < std::chrono::milliseconds{ 1 }) {
			return 1;
		}
		return static_cast<int>(std::min(milliseconds, wait_slice).count());
	}

	auto kill_process_group(const qint64 aProcessGroupId) noexcept -> void
	{
#if defined(Q_OS_UNIX)
		if (
			aProcessGroupId > 1
			&& aProcessGroupId
				<= static_cast<qint64>(
					std::numeric_limits<pid_t>::max())) {
			const auto processGroup = -static_cast<pid_t>(aProcessGroupId);
			static_cast<void>(::kill(processGroup, SIGKILL));
		}
#else
		static_cast<void>(aProcessGroupId);
#endif
	}

	auto resolve_process_group(
		const QProcess& aProcess,
		const qint64    aKnownProcessGroupId) noexcept -> qint64
	{
		if (aKnownProcessGroupId > 1) {
			return aKnownProcessGroupId;
		}
		const auto processId = aProcess.processId();
		return processId > 1 ? processId : 0;
	}

	auto kill_process_tree(QProcess& aProcess, const qint64 aProcessGroupId) -> void
	{
		kill_process_group(aProcessGroupId);
		if (aProcess.state() != QProcess::NotRunning) {
			aProcess.kill();
		}
	}

	auto kill_process_group_and_reap(
		QProcess&                       aProcess,
		const qint64                    aKnownProcessGroupId,
		const std::chrono::milliseconds aTimeout,
		const process_tree_kill&        aKillTree) -> bool
	{
		const auto deadline       = supervision_clock::now() + aTimeout;
		auto       processGroupId = resolve_process_group(aProcess, aKnownProcessGroupId);
		aKillTree(aProcess, processGroupId);
		while (aProcess.state() != QProcess::NotRunning) {
			const auto wait = wait_milliseconds(deadline);
			if (wait == 0) {
				return false;
			}
			static_cast<void>(aProcess.waitForFinished(wait));
			processGroupId = resolve_process_group(aProcess, processGroupId);
			aKillTree(aProcess, processGroupId);
		}
		kill_process_group(processGroupId);
		return true;
	}

	auto finish_background_reap(
		QProcess&                       aProcess,
		const qint64                    aKnownProcessGroupId,
		const std::chrono::milliseconds aTimeout,
		const process_tree_kill&        aKillTree) -> bool
	{
		const auto deadline       = supervision_clock::now() + aTimeout;
		auto       processGroupId = resolve_process_group(aProcess, aKnownProcessGroupId);
		while (aProcess.state() != QProcess::NotRunning) {
			const auto wait = wait_milliseconds(deadline);
			if (wait == 0) {
				return false;
			}
			processGroupId = resolve_process_group(aProcess, processGroupId);
			aKillTree(aProcess, processGroupId);
			static_cast<void>(aProcess.waitForFinished(wait));
		}
		kill_process_group(processGroupId);
		return true;
	}

	auto abandon_unreaped_process(std::unique_ptr<QProcess>& aProcess) noexcept -> void
	{
		static_cast<void>(
			QObject::disconnect(
				aProcess.get(),
				nullptr,
				aProcess.get(),
				nullptr));
		static_cast<void>(aProcess.release());
	}
}

namespace bpr::adapters::process_support {
#if defined(Q_OS_WIN)
	namespace {
		auto cpu_time(HANDLE aProcess) noexcept -> std::optional<std::uint64_t>
		{
			FILETIME created{ }, exited{ }, kernel{ }, user{ };
			if (!GetProcessTimes(aProcess, &created, &exited, &kernel, &user)) {
				return std::nullopt;
			}
			const auto ticks = [](FILETIME aTime) { return (static_cast<std::uint64_t>(aTime.dwHighDateTime) << 32) | aTime.dwLowDateTime; };
			return (ticks(kernel) + ticks(user)) / 10;
		}
	}
#endif
	auto self_process_counters() noexcept -> std::optional<process_counters>
	{
		process_counters result;
#if defined(Q_OS_MACOS)
		rusage_info_v4 usage{ };
		if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&usage)) != 0) {
			return std::nullopt;
		}
		timespec time{ };
		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0) {
			return std::nullopt;
		}
		result.CpuMicroseconds = static_cast<std::uint64_t>(time.tv_sec) * 1000000 + static_cast<std::uint64_t>(time.tv_nsec) / 1000;
		result.Wakeups         = usage.ri_pkg_idle_wkups + usage.ri_interrupt_wkups;
#elif defined(Q_OS_WIN)
		const auto cpu = cpu_time(GetCurrentProcess());
		if (!cpu) {
			return std::nullopt;
		}
		result.CpuMicroseconds = *cpu;
#else
		timespec time{ };
		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0) {
			return std::nullopt;
		}
		result.CpuMicroseconds = static_cast<std::uint64_t>(time.tv_sec) * 1000000 + static_cast<std::uint64_t>(time.tv_nsec) / 1000;
#endif
		return result;
	}
	void observe_reaped_cpu(QProcess& aProcess, std::function<void(std::optional<std::uint64_t>)> aObserve)
	{
#if defined(Q_OS_WIN)
		// Retain this exact kernel object before Qt closes its own handle at reap.
		auto handle = std::shared_ptr<void>{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(aProcess.processId())), [](void* aHandle) { if (aHandle) { CloseHandle(aHandle); } } };
		QObject::connect(&aProcess, &QProcess::finished, &aProcess, [handle, observe = std::move(aObserve)](int, QProcess::ExitStatus) { observe(handle ? cpu_time(handle.get()) : std::nullopt); }, Qt::DirectConnection);
#else
		// Qt's Unix forkfd reap discards rusage. A finished signal cannot recover it.
		// Keep the missing observation visible until a qualified per-child reap seam exists.
		QObject::connect(&aProcess, &QProcess::finished, &aProcess, [observe = std::move(aObserve)](int, QProcess::ExitStatus) { observe(std::nullopt); }, Qt::DirectConnection);
#endif
	}
}
