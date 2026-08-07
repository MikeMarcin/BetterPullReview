#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace bpr::adapters::agent {
	struct agent_process_launcher::implementation
	{
		QProcess Process;
		bool     Cancelled = false;
	};

	agent_process_launcher::agent_process_launcher(QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
	}

	agent_process_launcher::~agent_process_launcher()
	{
		cancel();
	}

	auto agent_process_launcher::run(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds) -> agent_run_result
	{
		Implementation->Cancelled = false;

		// Constructed empty. `QProcessEnvironment::systemEnvironment()` is the
		// convenient call and is the one that would hand the child every secret
		// this process holds; there is deliberately no path to it here.
		auto environment = QProcessEnvironment{ };
		for (const auto& [name, value] : aRequest.Environment) {
			environment.insert(
				QString::fromStdString(name), QString::fromStdString(value));
		}
		Implementation->Process.setProcessEnvironment(environment);

		auto arguments = QStringList{ };
		for (const auto& argument : aRequest.Arguments) {
			arguments << QString::fromStdString(argument);
		}
		// A list, never a command string: no shell sees this, so nothing in it
		// can be re-parsed into something else.
		Implementation->Process.start(
			QString::fromStdString(aRequest.Executable), arguments);
		if (!Implementation->Process.waitForStarted(aTimeoutMilliseconds)) {
			return { .Outcome = agent_run_outcome::failed_to_start };
		}

		// Everything the agent is told goes here.
		Implementation->Process.write(
			QByteArray::fromStdString(aRequest.Input));
		Implementation->Process.closeWriteChannel();

		if (!Implementation->Process.waitForFinished(aTimeoutMilliseconds)) {
			cancel();
			return { .Outcome = agent_run_outcome::cancelled };
		}
		if (Implementation->Cancelled) {
			// Nothing published. A partial answer from an agent is
			// indistinguishable from a complete one that happened to be short.
			return { .Outcome = agent_run_outcome::cancelled };
		}
		return {
			.Outcome = agent_run_outcome::completed,
			.Output  = Implementation->Process.readAllStandardOutput().toStdString(),
		};
	}

	void agent_process_launcher::cancel()
	{
		if (Implementation->Process.state() == QProcess::NotRunning) {
			return;
		}
		Implementation->Cancelled = true;
		Implementation->Process.kill();
		static_cast<void>(Implementation->Process.waitForFinished(5'000));
	}
} // namespace bpr::adapters::agent
