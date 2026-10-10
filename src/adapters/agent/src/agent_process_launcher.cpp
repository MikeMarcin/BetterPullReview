#include "bpr/adapters/agent/agent_process_launcher.hpp"

#include "agent_protocol_loop.hpp"

#include <QDir>
#include <QTemporaryDir>

#if defined(Q_OS_WIN)
	#include "windows_agent_process.hpp"
#else
	#include <QByteArray>
	#include <QCoreApplication>
	#include <QElapsedTimer>
	#include <QProcess>
	#include <QProcessEnvironment>
	#include <QString>
	#include <QStringList>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <fcntl.h>
	#include <poll.h>
	#include <sys/file.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
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
			std::array<int, 2> Lifetime{ -1, -1 };
			int                WorkspaceGuard = -1;
			void               close_owner() noexcept
			{
				if (Lifetime[0] >= 0) {
					static_cast<void>(::close(Lifetime[0]));
					Lifetime[0] = -1;
				}
			}
			~process_state()
			{
				close_owner();
				if (Lifetime[1] >= 0) {
					static_cast<void>(::close(Lifetime[1]));
				}
				if (Process.state() != QProcess::NotRunning) {
					static_cast<void>(Process.waitForFinished(5'000));
				}
				if (WorkspaceGuard >= 0) {
					static_cast<void>(::close(WorkspaceGuard));
				}
			}
	#endif
		};

	#if defined(Q_OS_UNIX)
		[[nodiscard]] auto open_workspace_guard(const application::agent_workspace_lifetime_guard& aGuard) -> int
		{
			if (aGuard.Path.empty() || aGuard.Path.size() > 4096U || aGuard.Path.find('\0') != std::string::npos) {
				return -1;
			}
			const auto      path = std::filesystem::path{ aGuard.Path };
			std::error_code error;
			if (!path.is_absolute() || std::filesystem::canonical(path, error) != path || error) {
				return -1;
			}
			const auto opened = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			if (opened < 0) {
				return -1;
			}
			// Keep the guard source above the helper's fixed FD 3/4 slots.
			const auto descriptor = ::fcntl(opened, F_DUPFD_CLOEXEC, 5);
			static_cast<void>(::close(opened));
			if (descriptor < 0) {
				return -1;
			}
			struct stat before{ }, named{ };
			const auto  valid = ::fstat(descriptor, &before) == 0
				&& S_ISREG(before.st_mode) && before.st_uid == ::geteuid()
				&& (before.st_mode & (S_IRWXG | S_IRWXO)) == 0 && before.st_nlink == 1
				&& aGuard.FileIdentity == std::array<std::uint64_t, 4U>{ 1U, static_cast<std::uint64_t>(before.st_dev), 0U, static_cast<std::uint64_t>(before.st_ino) }
				&& ::flock(descriptor, LOCK_SH | LOCK_NB) == 0
				&& ::lstat(path.c_str(), &named) == 0 && named.st_dev == before.st_dev && named.st_ino == before.st_ino;
			if (!valid) {
				static_cast<void>(::close(descriptor));
				return -1;
			}
			return descriptor;
		}
	#endif

		[[nodiscard]] auto wait_slice(
			const QElapsedTimer& aElapsed, const int aTimeoutMilliseconds) -> int
		{
			const auto remaining = static_cast<qint64>(aTimeoutMilliseconds)
				- aElapsed.elapsed();
			return static_cast<int>(std::clamp<qint64>(remaining, 0, 10));
		}

		template <typename State>
		auto end_process_tree(State& aState) -> bool
		{
	#if defined(Q_OS_UNIX)
			aState.close_owner();
			if (aState.Process.state() == QProcess::NotRunning || aState.Process.waitForFinished(5'000)) {
				return true;
			}
			aState.Process.kill();
			static_cast<void>(aState.Process.waitForFinished(5'000));
			return false;
	#else
			if (aState.Process.state() != QProcess::NotRunning) {
				aState.Process.kill();
				static_cast<void>(aState.Process.waitForFinished(5'000));
			}
			return aState.Process.state() == QProcess::NotRunning;
	#endif
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
				if (Cancelled()) {
					return detail::agent_protocol_write_status::cancelled;
				}
				const auto bytes = QByteArray{
					aBytes.data(), static_cast<qsizetype>(aBytes.size())
				};
				if (State.Process.write(bytes) != bytes.size()) {
					static_cast<void>(end_process_tree(State));
					return detail::agent_protocol_write_status::failed;
				}
				// QProcess::write only queues bytes. Keep the source claim until Qt
				// has drained its buffer into the child pipe or the child is stopped.
				while (State.Process.bytesToWrite() > 0) {
					if (Cancelled()) {
						static_cast<void>(end_process_tree(State));
						return detail::agent_protocol_write_status::cancelled;
					}
					const auto slice = wait_slice(Elapsed, TimeoutMilliseconds);
					if (slice == 0) {
						static_cast<void>(end_process_tree(State));
						return detail::agent_protocol_write_status::timeout;
					}
					if (!State.Process.waitForBytesWritten(slice) && State.Process.state() == QProcess::NotRunning) {
						return detail::agent_protocol_write_status::process_exited;
					}
					if (State.Process.error() == QProcess::WriteError) {
						static_cast<void>(end_process_tree(State));
						return detail::agent_protocol_write_status::failed;
					}
				}
				return detail::agent_protocol_write_status::success;
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
				return end_process_tree(State);
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
		const application::agent_launch_request& aGiven,
		const int                                aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const std::stop_token                    aStop)
		-> application::agent_run_result
	{
		// [[BPR-1375]]. A tool-free writing request runs in an empty folder of
		// its own, made here and removed when the run ends, rather than in the
		// review workspace: nothing in the reviewed repository (a CLAUDE.md, a
		// `.claude` or `.mcp.json`, a Git configuration) is where the agent
		// starts, and nothing the agent leaves behind outlives the request.
		auto writingFolder  = std::optional<QTemporaryDir>{ };
		auto writingRequest = std::optional<application::agent_launch_request>{ };
		if (aGiven.WritingOnly) {
			writingFolder.emplace(QDir{ QDir::tempPath() }.filePath(QStringLiteral("bpr-writing-XXXXXX")));
			if (!writingFolder->isValid()) {
				return { .Session = aGiven.Session, .Outcome = application::agent_run_outcome::failed_to_start, .StandardError = "tool-free writing folder could not be created", .Run = aGiven.Run };
			}
			writingRequest                   = aGiven;
			writingRequest->WorkingDirectory = writingFolder->path().toStdString();
		}
		const auto& aRequest = writingRequest ? *writingRequest : aGiven;
		auto        result   = application::agent_run_result{ .Session = aRequest.Session, .Run = aRequest.Run };
		if (const auto admitted = application::admit_agent_input(aRequest); !admitted) {
			result.Outcome         = application::agent_run_outcome::protocol_failure;
			result.ProtocolFailure = admitted.error() == application::agent_launch_failure::input_too_large ? application::agent_protocol_failure::input_too_large : application::agent_protocol_failure::invalid_runtime_metadata;
			return result;
		}
		result.Metadata                   = aRequest.Metadata;
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

#if !defined(Q_OS_UNIX) && !defined(Q_OS_WIN)
		if (aRequest.WorkspaceLifetimeGuard.has_value()) {
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = "workspace lifetime guard is unavailable on this platform";
			return result;
		}
#endif
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
		if (::socketpair(AF_UNIX, SOCK_STREAM, 0, state.Lifetime.data()) != 0
			|| ::fcntl(state.Lifetime[0], F_SETFD, FD_CLOEXEC) != 0
			|| ::fcntl(state.Lifetime[1], F_SETFD, FD_CLOEXEC) != 0) {
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = "agent lifetime channel could not be created";
			return result;
		}
		if (aRequest.WorkspaceLifetimeGuard.has_value()) {
			state.WorkspaceGuard = open_workspace_guard(*aRequest.WorkspaceLifetimeGuard);
			if (state.WorkspaceGuard < 0) {
				result.Outcome       = application::agent_run_outcome::failed_to_start;
				result.StandardError = "workspace lifetime guard could not be retained";
				return result;
			}
		}
		auto* const process        = &state.Process;
		const auto  channel        = state.Lifetime[1];
		const auto  workspaceGuard = state.WorkspaceGuard;
		state.Process.setChildProcessModifier([process, channel, workspaceGuard] {
			// A separate session excludes application-group signals and the
			// orphaned-stopped-group SIGHUP when the application dies.
			if (::setsid() < 0) {
				process->failChildProcessModifier("agent supervisor session", errno);
			}
			if (::dup2(channel, 3) < 0 || ::fcntl(3, F_SETFD, 0) != 0) {
				process->failChildProcessModifier("agent lifetime channel", errno);
			}
			if (workspaceGuard >= 0 && (::dup2(workspaceGuard, 4) < 0 || ::fcntl(4, F_SETFD, 0) != 0)) {
				process->failChildProcessModifier("workspace lifetime guard", errno);
			}
		});
		state.Process.setUnixProcessParameters({
			.flags                       = QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::ResetSignalHandlers,
			.lowestFileDescriptorToClose = workspaceGuard >= 0 ? 5 : 4,
		});
	#endif

		auto arguments = QStringList{ };
		for (const auto& argument : aRequest.Arguments) {
			arguments << QString::fromStdString(argument);
		}
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
	#if defined(Q_OS_UNIX)
		arguments.prepend(QString::fromStdString(aRequest.Executable));
		if (workspaceGuard >= 0) {
			arguments.prepend("--workspace-lease");
		}
		#ifdef BPR_AGENT_SUPERVISOR_EXECUTABLE
		const auto supervisor = QString::fromUtf8(BPR_AGENT_SUPERVISOR_EXECUTABLE);
		#else
		const auto supervisor = QCoreApplication::applicationDirPath() + "/better_pull_review_agent_supervisor";
		#endif
		state.Process.start(supervisor, arguments);
		static_cast<void>(::close(state.Lifetime[1]));
		state.Lifetime[1] = -1;
	#else
		state.Process.start(QString::fromStdString(aRequest.Executable), arguments);
	#endif
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
		if (state.Process.state() == QProcess::NotRunning
	#if defined(Q_OS_UNIX)
			&& state.Process.error() == QProcess::FailedToStart
	#endif
		) {
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = state.Process.errorString().toStdString();
			return result;
		}
	#if defined(Q_OS_UNIX)
		std::int32_t launchError = 0;
		std::size_t  received    = 0U;
		while (received < sizeof(launchError)) {
			if (cancelled() || elapsed.elapsed() >= aTimeoutMilliseconds) {
				static_cast<void>(end_process_tree(state));
				result.Outcome = cancelled() ? application::agent_run_outcome::cancelled : application::agent_run_outcome::timeout;
				return result;
			}
			pollfd     descriptor{ state.Lifetime[0], POLLIN, 0 };
			const auto polled = ::poll(&descriptor, 1, wait_slice(elapsed, aTimeoutMilliseconds));
			if ((polled < 0 && errno == EINTR) || polled == 0) {
				continue;
			}
			const auto count = polled > 0 ? ::read(state.Lifetime[0], reinterpret_cast<char*>(&launchError) + received, sizeof(launchError) - received) : -1;
			if (count < 0 && errno == EINTR) {
				continue;
			}
			if (count <= 0) {
				launchError = EIO;
				break;
			}
			received += static_cast<std::size_t>(count);
		}
		if (launchError != 0) {
			static_cast<void>(end_process_tree(state));
			result.Outcome       = application::agent_run_outcome::failed_to_start;
			result.StandardError = std::strerror(launchError);
			return result;
		}
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

	auto inspect_claude_process_wrapper(const std::string_view aPath) -> std::optional<core::agent_wrapper_refusal>
	{
#if defined(Q_OS_WIN)
		constexpr auto platform = core::agent_wrapper_platform::windows;
#else
		constexpr auto platform = core::agent_wrapper_platform::posix;
#endif
		if (const auto refusal = core::inspect_agent_wrapper_text(aPath, platform)) {
			return refusal;
		}
		try {
			const auto path   = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() } };
			auto       error  = std::error_code{ };
			const auto status = std::filesystem::status(path, error);
			if (error || status.type() == std::filesystem::file_type::not_found) {
				return core::agent_wrapper_refusal::missing;
			}
			if (status.type() == std::filesystem::file_type::directory) {
				return core::agent_wrapper_refusal::directory;
			}
			if (status.type() != std::filesystem::file_type::regular) {
				return core::agent_wrapper_refusal::not_regular_file;
			}
#if defined(Q_OS_UNIX)
			// The kernel's own answer for this user, groups and ACLs included,
			// rather than a reading of the mode bits.
			if (::access(path.c_str(), X_OK) != 0) {
				return core::agent_wrapper_refusal::not_executable;
			}
#endif
		} catch (...) {
			return core::agent_wrapper_refusal::missing;
		}
		return std::nullopt;
	}

	namespace {
#if defined(Q_OS_WIN)
		constexpr auto wrapper_platform = core::agent_wrapper_platform::windows;
#else
		constexpr auto wrapper_platform = core::agent_wrapper_platform::posix;
#endif
	}

	auto locate_claude(const std::string_view aPathVariable, const std::span<const std::string> aUsualFolders) -> std::optional<core::claude_search_candidate>
	{
		for (auto& candidate : core::claude_search_candidates(aPathVariable, aUsualFolders, wrapper_platform)) {
			if (!inspect_claude_process_wrapper(candidate.Path)) {
				return std::move(candidate);
			}
		}
		return std::nullopt;
	}

	auto locate_claude(const std::string_view aPathVariable, const std::string_view aHome) -> std::optional<core::claude_search_candidate>
	{
		return locate_claude(aPathVariable, core::usual_claude_folders(aHome, wrapper_platform));
	}

#if defined(BPR_BUILD_TESTING) && defined(Q_OS_WIN)
	void agent_process_launcher::force_job_assignment_failure_for_testing(
		const bool aEnabled) noexcept
	{
		detail::force_windows_job_assignment_failure_for_testing(aEnabled);
	}
#endif
} // namespace bpr::adapters::agent
