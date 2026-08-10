#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <string_view>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::agent {
	struct agent_process_launcher::implementation
	{
		QProcess         Process;
		std::atomic_bool CancelRequested = false;
		std::atomic_bool Running         = false;
#if defined(Q_OS_UNIX)
		qint64 ProcessGroup = 0;
#endif
	};

	namespace {
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
	} // namespace

	agent_process_launcher::agent_process_launcher(QObject* const aParent)
		: QObject{ aParent }
		, Implementation{ std::make_unique<implementation>() }
	{
	}

	agent_process_launcher::~agent_process_launcher()
	{
		Implementation->CancelRequested = true;
		end_process_tree(*Implementation);
	}

	auto agent_process_launcher::run(
		const application::agent_launch_request& aRequest,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink)
		-> application::agent_run_result
	{
		auto result = application::agent_run_result{
			.Session = aRequest.Session,
		};
#if defined(Q_OS_WIN)
		static_cast<void>(aTimeoutMilliseconds);
		static_cast<void>(aSink);
		result.Outcome = application::agent_run_outcome::failed_to_start;
		result.StandardError
			= "agent launch is disabled on Windows until pre-start process-tree "
			  "containment has target-native proof";
		return result;
#else
		auto& state = *Implementation;
		if (state.Running.exchange(true)) {
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
		} guard{ state.Running };
		state.CancelRequested = false;

		auto environment = QProcessEnvironment{ };
		for (const auto& [name, value] : aRequest.Environment) {
			environment.insert(
				QString::fromStdString(name), QString::fromStdString(value));
		}
		state.Process.setProcessEnvironment(environment);
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
			if (state.CancelRequested) {
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

		auto writeInput = [&state](const std::string_view aInput) {
			const auto bytes
				= QByteArray{ aInput.data(), static_cast<qsizetype>(aInput.size()) };
			return state.Process.write(bytes) == bytes.size();
		};
		if (!writeInput(aRequest.Input)) {
			end_process_tree(state);
			result.Outcome = application::agent_run_outcome::protocol_failure;
			result.ProtocolFailure
				= application::agent_protocol_failure::malformed_frame;
			return result;
		}
		const auto isAppServer
			= aRequest.Protocol == core::agent_protocol::codex_app_server;
		if (!isAppServer) {
			state.Process.closeWriteChannel();
		}

		auto decoder  = application::agent_protocol_decoder{ aRequest.Protocol };
		auto sequence = std::uint64_t{ 0U };
		auto protocolFailure
			= std::optional<application::agent_protocol_failure>{ };
		auto appServerPhase = std::uint8_t{ 0U };
		auto publish        = [&](const application::agent_protocol_event& aMessage) {
			auto event = application::agent_stream_event{
				.Session  = aRequest.Session,
				.Sequence = sequence++,
				.Text     = aMessage.Text,
			};
			result.Events.push_back(event);
			if (aSink) {
				try {
					aSink(event);
				} catch (...) {
					return false;
				}
			}
			return true;
		};
		auto drain = [&] {
			const auto error = state.Process.readAllStandardError();
			if (
				static_cast<std::size_t>(error.size())
				> application::maximum_agent_stderr_bytes
					- result.StandardError.size()) {
				protocolFailure = application::agent_protocol_failure::output_too_large;
				return;
			}
			result.StandardError += error.toStdString();

			const auto output = state.Process.readAllStandardOutput();
			if (output.isEmpty()) {
				return;
			}
			const auto decoded = decoder.push(std::string_view{
				output.constData(), static_cast<std::size_t>(output.size()) });
			if (!decoded) {
				protocolFailure = decoded.error();
				return;
			}
			for (const auto& message : *decoded) {
				if (message.Kind == application::agent_protocol_event_kind::output) {
					if (!publish(message)) {
						protocolFailure
							= application::agent_protocol_failure::malformed_frame;
						return;
					}
				} else {
					const auto expectedPhase
						= message.Kind
							== application::agent_protocol_event_kind::initialization_complete
						? 0U
						: 1U;
					if (!isAppServer || appServerPhase != expectedPhase) {
						protocolFailure
							= application::agent_protocol_failure::malformed_frame;
						return;
					}
					const auto followup = application::build_agent_protocol_followup(
						aRequest, message);
					if (!followup || !writeInput(*followup)) {
						protocolFailure = followup
							? application::agent_protocol_failure::malformed_frame
							: followup.error();
						return;
					}
					const auto acknowledged
						= decoder.acknowledge_followup(message.Kind);
					if (!acknowledged) {
						protocolFailure = acknowledged.error();
						return;
					}
					++appServerPhase;
					if (appServerPhase == 2U) {
						state.Process.closeWriteChannel();
					}
				}
			}
		};

		while (state.Process.state() != QProcess::NotRunning) {
			drain();
			if (protocolFailure) {
				end_process_tree(state);
				result.Events.clear();
				result.Outcome         = application::agent_run_outcome::protocol_failure;
				result.ProtocolFailure = protocolFailure;
				return result;
			}
			if (state.CancelRequested) {
				end_process_tree(state);
				result.Events.clear();
				result.Outcome = application::agent_run_outcome::cancelled;
				return result;
			}
			if (elapsed.elapsed() >= aTimeoutMilliseconds) {
				end_process_tree(state);
				result.Events.clear();
				result.Outcome = application::agent_run_outcome::timeout;
				return result;
			}
			static_cast<void>(state.Process.waitForReadyRead(
				wait_slice(elapsed, aTimeoutMilliseconds)));
		}
		drain();
		const auto trailing = decoder.finish();
		if (!trailing) {
			protocolFailure = trailing.error();
		}
		if (
			protocolFailure || !trailing
			|| (isAppServer && appServerPhase != 2U)) {
			end_process_tree(state);
			result.Events.clear();
			result.Outcome         = application::agent_run_outcome::protocol_failure;
			result.ProtocolFailure = protocolFailure.value_or(
				application::agent_protocol_failure::malformed_frame);
			return result;
		}
		for (const auto& message : *trailing) {
			if (
				message.Kind != application::agent_protocol_event_kind::output
				|| !publish(message)) {
				end_process_tree(state);
				result.Events.clear();
				result.Outcome = application::agent_run_outcome::protocol_failure;
				result.ProtocolFailure
					= application::agent_protocol_failure::malformed_frame;
				return result;
			}
		}

		result.ExitCode          = state.Process.exitCode();
		const auto exitedCleanly = state.Process.exitStatus() == QProcess::NormalExit
			&& *result.ExitCode == 0;
		end_process_tree(state);
		if (!exitedCleanly) {
			result.Events.clear();
			result.Outcome = application::agent_run_outcome::nonzero_exit;
			return result;
		}
		result.Outcome = application::agent_run_outcome::success;
		return result;
#endif
	}

	void agent_process_launcher::cancel() noexcept
	{
		Implementation->CancelRequested = true;
	}
} // namespace bpr::adapters::agent
