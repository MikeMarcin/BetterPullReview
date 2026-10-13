#include "bpr/adapters/process_support/process_supervision.hpp"

#include <QObject>

#include <algorithm>
#include <limits>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <sys/types.h>
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
