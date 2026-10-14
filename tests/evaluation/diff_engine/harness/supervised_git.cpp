// [[RFC-BPR-0082]] [[BPR-1466]]. See supervised_git.hpp. The launch flags,
// environment isolation, wait slice and kill-and-reap mirror
// src/adapters/git/src/installed_git_process.cpp's launch loop, through the
// same process_support functions; validation and typed results stay the
// adapter's and are not repeated here.

#include "harness/supervised_git.hpp"

#include "bpr/adapters/process_support/process_supervision.hpp"
#include "bpr/platform/unix_descriptor_sweep.hpp"

#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <chrono>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace bpr::evaluation {
	namespace process_support = bpr::adapters::process_support;

	auto run_supervised_git(const std::string& aGit, const std::vector<std::string>& aArguments, const std::string& aDirectory,
							const std::atomic_bool* const aStop) -> supervised_run
	{
		constexpr auto run_timeout  = std::chrono::minutes{ 10 };
		constexpr auto reap_timeout = std::chrono::milliseconds{ 2'000 };
		auto           result       = supervised_run{ };
		auto           arguments    = QStringList{ };
		for (const auto& argument : aArguments) {
			arguments.push_back(QString::fromStdString(argument));
		}
		auto environment = QProcessEnvironment{ };
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert("GIT_CONFIG_GLOBAL", "/dev/null");
		environment.insert("GIT_ATTR_NOSYSTEM", "1");
		environment.insert("GIT_TERMINAL_PROMPT", "0");
		environment.insert("HOME", QString::fromStdString(aDirectory));
		environment.insert("LC_ALL", "C");
		environment.insert("PATH", "/usr/bin:/bin");

		auto process = QProcess{ };
		process.setProgram(QString::fromStdString(aGit));
		process.setArguments(arguments);
		process.setProcessEnvironment(environment);
		process.setWorkingDirectory(QString::fromStdString(aDirectory));
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.setStandardInputFile(QProcess::nullDevice());
		process.setStandardErrorFile(QProcess::nullDevice());
		process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps
										 | QProcess::UnixProcessFlag::ResetSignalHandlers | QProcess::UnixProcessFlag::CreateNewSession);
		process.setChildProcessModifier([] {
			static_cast<void>(::umask(S_IRWXG | S_IRWXO));
			// [[BPR-1252]] As the Git adapter does: one launch per command, so
			// the child's descriptor sweep must stay bounded.
			bpr::platform::unix_descriptor_sweep::bound();
		});

		const auto deadline = process_support::supervision_clock::now() + run_timeout;
		const auto stopped  = [aStop] { return aStop != nullptr && aStop->load(std::memory_order_acquire); };
		auto       output   = QByteArray{ };
		process.start();
		if (!process.waitForStarted(5'000)) {
			result.Returned = std::chrono::steady_clock::now();
			return result;
		}
		const auto group = process_support::resolve_process_group(process, 0);
		for (;;) {
			if (stopped()) {
				result.Reaped   = process_support::kill_process_group_and_reap(process, group, reap_timeout);
				result.Stopped  = true;
				result.Returned = std::chrono::steady_clock::now();
				return result;
			}
			output += process.readAllStandardOutput();
			if (process.state() == QProcess::NotRunning) {
				break;
			}
			if (process_support::supervision_clock::now() >= deadline) {
				result.Reaped   = process_support::kill_process_group_and_reap(process, group, reap_timeout);
				result.Returned = std::chrono::steady_clock::now();
				return result;
			}
			static_cast<void>(process.waitForReadyRead(process_support::wait_milliseconds(deadline)));
		}
		output += process.readAllStandardOutput();
		process_support::kill_process_group(group);
		result.Returned = std::chrono::steady_clock::now();
		if (process.exitStatus() != QProcess::NormalExit || (process.exitCode() != 0 && process.exitCode() != 1)) {
			return result;
		}
		result.Output = output.toStdString();
		return result;
	}
} // namespace bpr::evaluation
