#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include "agent_protocol_loop.hpp"

#if defined(Q_OS_WIN)
	#include "windows_agent_process.hpp"
#else
	#include <QByteArray>
	#include <QElapsedTimer>
	#include <QProcess>
	#include <QProcessEnvironment>
	#include <QString>
	#include <QStringList>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::agent {
	struct agent_process_launcher::implementation
	{
		std::atomic_uint64_t CancellationGeneration = 0U;
		std::atomic_bool     Running                = false;
	};

	namespace {
#if !defined(Q_OS_WIN)
		struct process_state
		{
			QProcess Process{ nullptr };
	#if defined(Q_OS_UNIX)
			qint64 ProcessGroup = 0;
	#endif
		};

		[[nodiscard]] auto wait_slice(
			const QElapsedTimer& aElapsed, const int aTimeoutMilliseconds) -> int
		{
			const auto remaining = static_cast<qint64>(aTimeoutMilliseconds)
				- aElapsed.elapsed();
			return static_cast<int>(std::clamp<qint64>(remaining, 0, 10));
		}

		template <typename State>
		void end_process_tree(State& aState)
		{
	#if defined(Q_OS_UNIX)
			if (aState.ProcessGroup > 0) {
				static_cast<void>(::kill(
					-static_cast<pid_t>(aState.ProcessGroup), SIGKILL));
				aState.ProcessGroup = 0;
			}
	#endif
			if (aState.Process.state() != QProcess::NotRunning) {
				aState.Process.kill();
				static_cast<void>(aState.Process.waitForFinished(5'000));
			}
		}

		class qt_agent_protocol_transport final
			: public detail::agent_protocol_transport
		{
		public:
			qt_agent_protocol_transport(
				process_state&               aState,
				const QElapsedTimer&         aElapsed,
				const int                    aTimeoutMilliseconds,
				const std::function<bool()>& aCancelled)
				: State{ aState }
				, Elapsed{ aElapsed }
				, TimeoutMilliseconds{ aTimeoutMilliseconds }
				, Cancelled{ aCancelled }
			{
			}

			[[nodiscard]] auto write(const std::string_view aBytes)
				-> detail::agent_protocol_write_status override
			{
				const auto bytes = QByteArray{
					aBytes.data(), static_cast<qsizetype>(aBytes.size())
				};
				return State.Process.write(bytes) == bytes.size()
					? detail::agent_protocol_write_status::success
					: detail::agent_protocol_write_status::failed;
			}

			void close_input() noexcept override
			{
				State.Process.closeWriteChannel();
			}

			[[nodiscard]] auto read_standard_output(std::span<char> aBuffer)
				-> detail::agent_protocol_pipe_read_result override
			{
				return read_available(
					QProcess::StandardOutput,
					StandardOutputBatchRemaining,
					aBuffer);
			}

			[[nodiscard]] auto read_standard_error(std::span<char> aBuffer)
				-> detail::agent_protocol_pipe_read_result override
			{
				return read_available(
					QProcess::StandardError,
					StandardErrorBatchRemaining,
					aBuffer);
			}

			[[nodiscard]] auto wait() -> detail::agent_protocol_wait_result override
			{
				if (State.Process.state() == QProcess::NotRunning) {
					return {
						.Status         = detail::agent_protocol_wait_status::exited,
						.ExitCode       = State.Process.exitCode(),
						.ExitedNormally = State.Process.exitStatus()
							== QProcess::NormalExit,
					};
				}
				if (Cancelled()) {
					return { .Status = detail::agent_protocol_wait_status::cancelled };
				}
				if (Elapsed.elapsed() >= TimeoutMilliseconds) {
					return { .Status = detail::agent_protocol_wait_status::timeout };
				}
				static_cast<void>(State.Process.waitForReadyRead(
					wait_slice(Elapsed, TimeoutMilliseconds)));
				return { };
			}

			[[nodiscard]] auto terminate() noexcept -> bool override
			{
				end_process_tree(State);
				return true;
			}

		private:
			[[nodiscard]] auto read_available(
				const QProcess::ProcessChannel aChannel,
				qint64&                        aBatchRemaining,
				std::span<char>                aBuffer) -> detail::agent_protocol_pipe_read_result
			{
				State.Process.setReadChannel(aChannel);
				if (aBatchRemaining == 0) {
					aBatchRemaining = State.Process.bytesAvailable();
				}
				if (aBatchRemaining <= 0) {
					aBatchRemaining = 0;
					return {
						.Status = State.Process.state() == QProcess::NotRunning
							? detail::agent_protocol_pipe_read_status::closed
							: detail::agent_protocol_pipe_read_status::no_data,
					};
				}
				const auto requested = std::min(
					static_cast<qint64>(aBuffer.size()), aBatchRemaining);
				const auto count = State.Process.read(aBuffer.data(), requested);
				if (count <= 0) {
					aBatchRemaining = 0;
					return { .Status = detail::agent_protocol_pipe_read_status::failed };
				}
				aBatchRemaining -= count;
				return {
					.Status        = detail::agent_protocol_pipe_read_status::data,
					.Count         = static_cast<std::size_t>(count),
					.BatchComplete = aBatchRemaining == 0,
				};
			}

			process_state&               State;
			const QElapsedTimer&         Elapsed;
			int                          TimeoutMilliseconds;
			const std::function<bool()>& Cancelled;
			qint64                       StandardOutputBatchRemaining = 0;
			qint64                       StandardErrorBatchRemaining  = 0;
		};
#endif
	} // namespace

	agent_process_launcher::agent_process_launcher(QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
	}

	agent_process_launcher::~agent_process_launcher()
	{
		Implementation->CancellationGeneration.fetch_add(
			1U,
			std::memory_order_acq_rel);
	}

	auto agent_process_launcher::run(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const std::stop_token                    aStop)
		-> application::agent_run_result
	{
		auto       result                 = application::agent_run_result{ .Session = aRequest.Session };
		auto&      control                = *Implementation;
		const auto cancellationGeneration = control.CancellationGeneration.load(
			std::memory_order_acquire);
		const auto cancelled = [&] {
			return aStop.stop_requested()
				|| control.CancellationGeneration.load(std::memory_order_acquire)
				!= cancellationGeneration;
		};
		if (cancelled()) {
			result.Outcome = application::agent_run_outcome::cancelled;
			return result;
		}
		if (control.Running.exchange(true)) {
			result.Outcome = application::agent_run_outcome::failed_to_start;
			return result;
		}
		struct running_guard
		{
			std::atomic_bool& Running;
			~running_guard()
			{
				Running = false;
			}
		} guard{ control.Running };
		std::error_code workingDirectoryError;
		const auto      workingDirectory = std::filesystem::path{
			aRequest.WorkingDirectory
		};
		if (
			workingDirectory.empty() || !workingDirectory.is_absolute()
			|| !std::filesystem::is_directory(
				workingDirectory,
				workingDirectoryError)
			|| workingDirectoryError) {
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = "agent working directory is not an accepted directory";
			return result;
		}

#if defined(Q_OS_WIN)
		return detail::run_windows_agent_process(
			aRequest, aTimeoutMilliseconds, aSink, cancelled);
#else
		auto state = process_state{ };

		auto environment = QProcessEnvironment{ };
		for (const auto& [name, value] : aRequest.Environment) {
			environment.insert(
				QString::fromStdString(name), QString::fromStdString(value));
		}
		state.Process.setProcessEnvironment(environment);
		state.Process.setWorkingDirectory(
			QString::fromStdString(aRequest.WorkingDirectory));
		state.Process.setProcessChannelMode(QProcess::SeparateChannels);
	#if defined(Q_OS_UNIX)
		auto* const process = &state.Process;
		state.Process.setChildProcessModifier([process] {
			if (::setpgid(0, 0) != 0) {
				process->failChildProcessModifier("setpgid", errno);
			}
		});
	#endif

		auto arguments = QStringList{ };
		for (const auto& argument : aRequest.Arguments) {
			arguments << QString::fromStdString(argument);
		}
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		state.Process.start(QString::fromStdString(aRequest.Executable), arguments);
		while (state.Process.state() == QProcess::Starting) {
			if (cancelled()) {
				end_process_tree(state);
				result.Outcome = application::agent_run_outcome::cancelled;
				return result;
			}
			if (elapsed.elapsed() >= aTimeoutMilliseconds) {
				end_process_tree(state);
				result.Outcome = application::agent_run_outcome::timeout;
				return result;
			}
			static_cast<void>(state.Process.waitForStarted(
				wait_slice(elapsed, aTimeoutMilliseconds)));
		}
		if (state.Process.state() == QProcess::NotRunning) {
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = state.Process.errorString().toStdString();
			return result;
		}
	#if defined(Q_OS_UNIX)
		state.ProcessGroup = state.Process.processId();
	#endif

		const auto cancellationProbe = std::function<bool()>{ cancelled };
		auto       transport         = qt_agent_protocol_transport{
			state, elapsed, aTimeoutMilliseconds, cancellationProbe
		};
		return detail::run_agent_protocol_loop(aRequest, aSink, transport);
#endif
	}

	void agent_process_launcher::cancel() noexcept
	{
		Implementation->CancellationGeneration.fetch_add(
			1U,
			std::memory_order_acq_rel);
	}

#if defined(BPR_BUILD_TESTING) && defined(Q_OS_WIN)
	void agent_process_launcher::force_job_assignment_failure_for_testing(
		const bool aEnabled) noexcept
	{
		detail::force_windows_job_assignment_failure_for_testing(aEnabled);
	}
#endif
} // namespace bpr::adapters::agent
