#include "../support/test_expectation.hpp"
#include "bpr/adapters/process_support/process_supervision.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#if defined(Q_OS_UNIX)
	#include <sys/resource.h>
#endif
int main(int aArgc, char** aArgv)
try {
	QCoreApplication app{ aArgc, aArgv };
	using namespace bpr;
	using tests::expect;
	if (app.arguments().contains(QStringLiteral("--energy-child"))) {
		QElapsedTimer timer;
		timer.start();
		while (timer.elapsed() < 80) { }
		return 0;
	}
	const auto before = adapters::process_support::self_process_counters();
#if defined(Q_OS_UNIX)
	rusage initial{ };
	expect(getrusage(RUSAGE_SELF, &initial) == 0, "independent native CPU baseline");
#endif
	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < 80) { }
	const auto after = adapters::process_support::self_process_counters();
	expect(before && after, "native process counters are available");
#if defined(Q_OS_UNIX)
	rusage final{ };
	expect(getrusage(RUSAGE_SELF, &final) == 0, "independent native CPU terminal");
	const auto micros   = [](const rusage& aUsage) { return static_cast<std::uint64_t>(aUsage.ru_utime.tv_sec + aUsage.ru_stime.tv_sec) * 1000000 + static_cast<std::uint64_t>(aUsage.ru_utime.tv_usec + aUsage.ru_stime.tv_usec); };
	const auto expected = micros(final) - micros(initial);
	const auto actual   = after->CpuMicroseconds - before->CpuMicroseconds;
	expect(actual + 5000 >= expected && actual <= expected + 5000, "native process clock agrees with independent getrusage within5ms");
#endif
	expect(before && after && after->CpuMicroseconds > before->CpuMicroseconds + 1000, "native process counters observe actual CPU work");
#if defined(Q_OS_MACOS)
	expect(before->Wakeups && after->Wakeups && *after->Wakeups >= *before->Wakeups, "macOS reports package idle plus interrupt wakeups");
#else
	expect(!after->Wakeups, "platforms without wakeup counters leave them absent");
#endif
	QTemporaryDir scratch{ QDir::tempPath() + QStringLiteral("/energy-native-XXXXXX") };
	expect(scratch.isValid(), "native course owns its scratch root");
	QProcess child;
	// Neither side consumes standard streams. Forward them rather than asking
	// QProcess to create private pipes inside the Windows AppContainer.
	child.setProcessChannelMode(QProcess::ForwardedChannels);
	child.setInputChannelMode(QProcess::ForwardedInputChannel);
	child.setProgram(QFileInfo{ app.applicationFilePath() }.canonicalFilePath());
	child.setArguments({ QStringLiteral("--energy-child") });
	child.setWorkingDirectory(scratch.path());
#if defined(Q_OS_UNIX)
	child.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps);
#endif
	child.start();
	expect(child.waitForStarted(3000), "the course starts only its canonical self child");
	bool                         reaped = false;
	std::optional<std::uint64_t> cpu;
	adapters::process_support::observe_reaped_cpu(child, [&](auto aCpu) { reaped = true; cpu = aCpu; });
	expect(child.waitForFinished(5000) && child.exitCode() == 0 && reaped, "native child terminal is observed once at reap");
#if defined(Q_OS_WIN)
	expect(cpu && *cpu > 1000, "the retained exact Windows process handle reports child CPU");
#else
	expect(!cpu, "Qt's Unix reap gap is explicit and cannot masquerade as measured zero");
#endif
	std::cout << "insights-native-metrics:pass self_cpu_delta=" << after->CpuMicroseconds - before->CpuMicroseconds << " wakeups_reported=" << after->Wakeups.has_value() << " child_cpu_reported=" << cpu.has_value() << '\n';
} catch (...) {
	return bpr::tests::failing_exit();
}
