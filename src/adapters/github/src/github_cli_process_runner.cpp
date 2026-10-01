#include "github_cli_process_runner.hpp"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(Q_OS_UNIX)
	#include <cerrno>
	#include <csignal>
	#include <sys/resource.h>
	#include <sys/types.h>
	#include <unistd.h>

	#include "bpr/platform/unix_descriptor_sweep.hpp"
#endif

#if defined(Q_OS_WIN)
	#include "bpr/platform/windows_standard_handles.hpp"
#endif

namespace bpr::adapters::github::detail {
	namespace {
		using clock = std::chrono::steady_clock;

		[[nodiscard]] auto directory_within_limits(const github_cli_process_file_limits& aLimits) -> bool
		{
			if (aLimits.Directory.empty()) {
				return aLimits.MaximumFileBytes == 0U;
			}
			std::error_code                                     error;
			std::uint64_t                                       bytes   = 0U;
			std::size_t                                         entries = 0U;
			std::filesystem::recursive_directory_iterator       iterator{ aLimits.Directory, error };
			const std::filesystem::recursive_directory_iterator end;
			while (!error && iterator != end) {
				if (++entries > aLimits.MaximumDirectoryEntries || iterator->is_symlink(error)) {
					return false;
				}
				if (iterator->is_regular_file(error)) {
					const auto size = iterator->file_size(error);
					// Git atomically replaces lock and temporary pack files while
					// this bounded sampler runs. A vanished file consumes no bytes;
					// file_size's failure sentinel is not an oversized file.
					if (error == std::errc::no_such_file_or_directory) {
						error.clear();
					} else if (error || size >= aLimits.MaximumFileBytes || size > aLimits.MaximumDirectoryBytes - bytes) {
						return false;
					} else {
						bytes += size;
					}
				}
				if (error == std::errc::no_such_file_or_directory) {
					error.clear();
				} else if (error) {
					return false;
				}
				iterator.increment(error);
			}
			return !error;
		}

		inline constexpr auto wait_slice = std::chrono::milliseconds{ 10 };

		[[nodiscard]] auto contains_nul(const std::string_view aValue) noexcept -> bool
		{
			return aValue.find('\0') != std::string_view::npos;
		}

		[[nodiscard]] auto valid_environment_name(
			const std::string_view aName) noexcept -> bool
		{
			if (aName.empty() || contains_nul(aName)) {
				return false;
			}

			const auto asciiAlpha = [](const char aValue) noexcept {
				return (aValue >= 'A' && aValue <= 'Z')
					|| (aValue >= 'a' && aValue <= 'z');
			};
			const auto asciiDigit = [](const char aValue) noexcept {
				return aValue >= '0' && aValue <= '9';
			};

			if (aName.front() != '_' && !asciiAlpha(aName.front())) {
				return false;
			}
			return std::all_of(
				aName.begin() + 1,
				aName.end(),
				[&](const char aValue) {
					return aValue == '_' || asciiAlpha(aValue)
						|| asciiDigit(aValue);
				});
		}

		[[nodiscard]] auto equal_environment_names(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			if (aLeft.size() != aRight.size()) {
				return false;
			}

			const auto foldAscii = [](const char aValue) noexcept {
				if (aValue >= 'A' && aValue <= 'Z') {
					return static_cast<char>(aValue - 'A' + 'a');
				}
				return aValue;
			};
			for (std::size_t index = 0; index < aLeft.size(); ++index) {
				if (foldAscii(aLeft[index]) != foldAscii(aRight[index])) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto add_bounded(
			std::size_t&      aTotal,
			const std::size_t aAmount,
			const std::size_t aLimit) noexcept -> bool
		{
			if (aTotal > aLimit || aAmount > aLimit - aTotal) {
				return false;
			}
			aTotal += aAmount;
			return true;
		}

		[[nodiscard]] auto valid_request_shape(
			const github_cli_process_request& aRequest) noexcept -> bool
		{
			if (
				aRequest.Executable.empty()
				|| aRequest.Executable.size() > github_cli_process_max_path_bytes
				|| contains_nul(aRequest.Executable)
				|| aRequest.WorkingDirectory.empty()
				|| aRequest.WorkingDirectory.size()
					> github_cli_process_max_path_bytes
				|| contains_nul(aRequest.WorkingDirectory)
				|| aRequest.Arguments.empty()
				|| aRequest.Arguments.size()
					> github_cli_process_max_argument_count
				|| aRequest.Environment.size()
					> github_cli_process_max_environment_count
				|| aRequest.StandardInputLimit == 0
				|| aRequest.StandardInputLimit
					> github_cli_process_max_standard_input_bytes
				|| aRequest.StandardInput.size() > aRequest.StandardInputLimit
				|| aRequest.StandardOutputLimit == 0
				|| aRequest.StandardOutputLimit
					> github_cli_process_max_standard_output_bytes
				|| aRequest.StandardErrorLimit == 0
				|| aRequest.StandardErrorLimit
					> github_cli_process_max_standard_error_bytes
				|| aRequest.Timeout <= std::chrono::milliseconds::zero()
				|| aRequest.Timeout
					> std::chrono::duration_cast<std::chrono::milliseconds>(
						github_cli_process_max_timeout)
				|| aRequest.ReapTimeout <= std::chrono::milliseconds::zero()
				|| aRequest.ReapTimeout
					> std::chrono::duration_cast<std::chrono::milliseconds>(
						github_cli_process_max_reap_timeout)) {
				return false;
			}

			std::size_t argumentBytes = 0;
			for (const auto& argument : aRequest.Arguments) {
				if (
					argument.empty()
					|| argument.size() > github_cli_process_max_argument_bytes
					|| contains_nul(argument)
					|| !add_bounded(
						argumentBytes,
						argument.size(),
						github_cli_process_max_arguments_bytes)) {
					return false;
				}
			}

			std::size_t environmentBytes = 0;
#if defined(_WIN32)
			bool hasEmptyPath  = false;
			bool hasSystemRoot = false;
#endif
			for (std::size_t index = 0; index < aRequest.Environment.size(); ++index) {
				const auto& entry = aRequest.Environment[index];
				if (
					entry.Name.size()
						> github_cli_process_max_environment_name_bytes
					|| !valid_environment_name(entry.Name)
					|| entry.Value.size()
						> github_cli_process_max_environment_value_bytes
					|| contains_nul(entry.Value)
					|| !add_bounded(
						environmentBytes,
						entry.Name.size(),
						github_cli_process_max_environment_bytes)
					|| !add_bounded(
						environmentBytes,
						entry.Value.size(),
						github_cli_process_max_environment_bytes)) {
					return false;
				}
#if defined(_WIN32)
				if (equal_environment_names(entry.Name, "PATH")) {
					hasEmptyPath = entry.Value.empty();
				}
				if (equal_environment_names(entry.Name, "SystemRoot")) {
					hasSystemRoot = !entry.Value.empty();
				}
#endif
				for (std::size_t prior = 0; prior < index; ++prior) {
					if (equal_environment_names(
							entry.Name,
							aRequest.Environment[prior].Name)) {
						return false;
					}
				}
			}
#if defined(_WIN32)
			if (!hasEmptyPath || !hasSystemRoot) {
				return false;
			}
#endif
			return true;
		}

		[[nodiscard]] auto decode_utf8(
			const std::string_view aValue,
			QString&               aDecoded) -> bool
		{
			if (aValue.size()
				> static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
				return false;
			}

			aDecoded = QString::fromUtf8(
				aValue.data(),
				static_cast<qsizetype>(aValue.size()));
			const auto encoded = aDecoded.toUtf8();
			return static_cast<std::size_t>(encoded.size()) == aValue.size()
				&& std::equal(
					   aValue.begin(),
					   aValue.end(),
					   encoded.constData());
		}

		[[nodiscard]] auto exact_canonical_file(
			const std::string_view aPath,
			QString&               aCanonical) -> bool
		{
			QString decoded;
			if (
				aPath.empty() || aPath.size() > github_cli_process_max_path_bytes
				|| contains_nul(aPath) || !decode_utf8(aPath, decoded)) {
				return false;
			}

			const QFileInfo information{ decoded };
			aCanonical = information.canonicalFilePath();
			return information.isAbsolute() && information.exists()
				&& information.isFile() && information.isExecutable()
				&& !aCanonical.isEmpty()
				&& QDir::cleanPath(QDir::fromNativeSeparators(decoded))
				== QDir::cleanPath(QDir::fromNativeSeparators(aCanonical));
		}

		[[nodiscard]] auto exact_canonical_neutral_directory(
			const std::string_view aPath,
			QString&               aCanonical) -> bool
		{
			QString decoded;
			if (
				aPath.empty() || aPath.size() > github_cli_process_max_path_bytes
				|| contains_nul(aPath) || !decode_utf8(aPath, decoded)) {
				return false;
			}

			const QFileInfo information{ decoded };
			aCanonical = information.canonicalFilePath();
			if (
				!information.isAbsolute() || !information.exists()
				|| !information.isDir() || aCanonical.isEmpty()
				|| QDir::cleanPath(QDir::fromNativeSeparators(decoded))
					!= QDir::cleanPath(QDir::fromNativeSeparators(aCanonical))) {
				return false;
			}

			QDir directory{ aCanonical };
			for (;;) {
				const QFileInfo dotGit{
					directory.filePath(QStringLiteral(".git"))
				};
				const QFileInfo head{
					directory.filePath(QStringLiteral("HEAD"))
				};
				const QFileInfo objects{
					directory.filePath(QStringLiteral("objects"))
				};
				const QFileInfo refs{
					directory.filePath(QStringLiteral("refs"))
				};
				const bool bareRepository = head.isFile() && objects.isDir() && refs.isDir();
				if (dotGit.exists() || bareRepository) {
					return false;
				}

				const auto current = directory.absolutePath();
				if (
					!directory.cdUp()
					|| directory.absolutePath() == current) {
					break;
				}
			}
			return true;
		}

		struct validated_process_request
		{
			QString             Executable;
			QStringList         Arguments;
			QProcessEnvironment Environment;
			QString             WorkingDirectory;
		};

		[[nodiscard]] auto validate_request(
			const github_cli_process_request& aRequest,
			validated_process_request&        aValidated) -> bool
		{
			if (
				!valid_request_shape(aRequest)
				|| !exact_canonical_file(
					aRequest.Executable,
					aValidated.Executable)
				|| !exact_canonical_neutral_directory(
					aRequest.WorkingDirectory,
					aValidated.WorkingDirectory)) {
				return false;
			}

			for (const auto& argument : aRequest.Arguments) {
				QString decoded;
				if (!decode_utf8(argument, decoded)) {
					return false;
				}
				aValidated.Arguments.push_back(std::move(decoded));
			}

			for (const auto& entry : aRequest.Environment) {
				QString name;
				QString value;
				if (!decode_utf8(entry.Name, name) || !decode_utf8(entry.Value, value)) {
					return false;
				}
				aValidated.Environment.insert(name, value);
			}
			return true;
		}

		[[nodiscard]] auto wait_milliseconds(
			const clock::time_point aDeadline) noexcept -> int
		{
			const auto remaining = aDeadline - clock::now();
			if (remaining <= clock::duration::zero()) {
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

		[[nodiscard]] auto resolve_process_group(
			const QProcess& aProcess,
			const qint64    aKnownProcessGroupId) noexcept -> qint64
		{
			if (aKnownProcessGroupId > 1) {
				return aKnownProcessGroupId;
			}
			const auto processId = aProcess.processId();
			return processId > 1 ? processId : 0;
		}

		[[nodiscard]] auto kill_process_group_and_reap(
			QProcess&                       aProcess,
			const qint64                    aKnownProcessGroupId,
			const std::chrono::milliseconds aTimeout) -> bool
		{
			const auto deadline       = clock::now() + aTimeout;
			auto       processGroupId = resolve_process_group(aProcess, aKnownProcessGroupId);
			kill_process_group(processGroupId);
			if (aProcess.state() != QProcess::NotRunning) {
				aProcess.kill();
			}

			while (aProcess.state() != QProcess::NotRunning) {
				const auto wait = wait_milliseconds(deadline);
				if (wait == 0) {
					return false;
				}
				static_cast<void>(aProcess.waitForFinished(wait));
				if (aProcess.state() != QProcess::NotRunning) {
					processGroupId = resolve_process_group(aProcess, processGroupId);
					kill_process_group(processGroupId);
					aProcess.kill();
				}
			}
			kill_process_group(processGroupId);
			return true;
		}

		auto finish_background_reap(
			QProcess&    aProcess,
			const qint64 aKnownProcessGroupId) -> void
		{
			auto processGroupId = resolve_process_group(aProcess, aKnownProcessGroupId);
			while (aProcess.state() != QProcess::NotRunning) {
				processGroupId = resolve_process_group(aProcess, processGroupId);
				kill_process_group(processGroupId);
				aProcess.kill();
				static_cast<void>(aProcess.waitForFinished(
					static_cast<int>(wait_slice.count())));
			}
			kill_process_group(processGroupId);
		}

		enum class process_stream : std::uint8_t {
			standard_output,
			standard_error,
		};

		[[nodiscard]] auto drain_channel(
			QProcess&            aProcess,
			const process_stream aStream,
			const std::size_t    aLimit,
			std::size_t&         aConsumed,
			std::string*         aRetainedOutput)
			-> std::optional<github_cli_process_failure>
		{
			const auto channel = aStream == process_stream::standard_output
				? QProcess::StandardOutput
				: QProcess::StandardError;
			const auto failure = aStream == process_stream::standard_output
				? github_cli_process_failure::standard_output_limit
				: github_cli_process_failure::standard_error_limit;
			aProcess.setReadChannel(channel);

			std::array<char, 16U * 1024U> buffer{ };
			while (aProcess.bytesAvailable() > 0) {
				const auto remaining = aLimit - aConsumed;
				const auto requested = std::min<std::size_t>(
					buffer.size(),
					remaining + 1U);
				const auto bytesRead = aProcess.read(
					buffer.data(),
					static_cast<qint64>(requested));
				if (bytesRead < 0) {
					return failure;
				}
				if (bytesRead == 0) {
					break;
				}

				const auto byteCount = static_cast<std::size_t>(bytesRead);
				if (byteCount > remaining) {
					return failure;
				}
				if (aRetainedOutput != nullptr) {
					aRetainedOutput->append(buffer.data(), byteCount);
				}
				aConsumed += byteCount;
			}
			return std::nullopt;
		}

		struct process_execution
		{
			github_cli_process_result Result;
			std::unique_ptr<QProcess> AwaitingReap;
			qint64                    ProcessGroupId = 0;
		};

		[[nodiscard]] auto failed_execution(
			std::unique_ptr<QProcess>&       aProcess,
			const qint64                     aKnownProcessGroupId,
			const std::chrono::milliseconds  aReapTimeout,
			const github_cli_process_failure aFailure) -> process_execution
		{
			const auto processGroupId = resolve_process_group(*aProcess, aKnownProcessGroupId);
			if (
				kill_process_group_and_reap(
					*aProcess,
					processGroupId,
					aReapTimeout)) {
				return {
					.Result         = std::unexpected{ aFailure },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			return {
				.Result = std::unexpected{
					github_cli_process_failure::reap_failure },
				.AwaitingReap   = std::move(aProcess),
				.ProcessGroupId = processGroupId,
			};
		}

		/// \p aFailStandardHandleList is the test seam's: on Windows it withholds
		/// the launch's standard error handle, as a null standard handle would,
		/// so the standard-handle list cannot be built and the launch is refused.
		[[nodiscard]] auto execute_process(
			const github_cli_process_request&     aRequest,
			const std::stop_token                 aStop,
			const std::atomic_bool&               aShutdownRequested,
			const clock::time_point               aDeadline,
			const github_cli_process_file_limits& aFileLimits,
			[[maybe_unused]] const bool           aFailStandardHandleList) -> process_execution
		{
			validated_process_request validated;
			try {
				if (!validate_request(aRequest, validated)) {
					return {
						.Result = std::unexpected{
							github_cli_process_failure::invalid_request },
						.AwaitingReap   = nullptr,
						.ProcessGroupId = 0,
					};
				}
			} catch (...) {
				return {
					.Result = std::unexpected{
						github_cli_process_failure::invalid_request },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			if (
				aShutdownRequested.load(std::memory_order_acquire)
				|| aStop.stop_requested()) {
				return {
					.Result = std::unexpected{
						github_cli_process_failure::cancelled },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}
			if (clock::now() >= aDeadline) {
				return {
					.Result = std::unexpected{
						github_cli_process_failure::timeout },
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			}

#if defined(Q_OS_WIN)
			bpr::platform::windows_standard_handles::launch_restriction inheritance;
#endif
			auto       process        = std::make_unique<QProcess>();
			const auto processGroupId = std::make_shared<qint64>(0);
			try {
				process->setProgram(validated.Executable);
				process->setArguments(validated.Arguments);
				process->setProcessEnvironment(validated.Environment);
				process->setWorkingDirectory(validated.WorkingDirectory);
				process->setProcessChannelMode(QProcess::SeparateChannels);
				process->setInputChannelMode(QProcess::ManagedInputChannel);
				if (!aFileLimits.StandardOutputFile.empty()) {
					process->setStandardOutputFile(QString::fromStdString(aFileLimits.StandardOutputFile.string()), QIODevice::Truncate);
				}
#if defined(Q_OS_UNIX)
				process->setChildProcessModifier([limit = aFileLimits.MaximumFileBytes, child = process.get()] {
					if (limit != 0U) {
						const rlimit fileLimit{ static_cast<rlim_t>(limit), static_cast<rlim_t>(limit) };
						if (::setrlimit(RLIMIT_FSIZE, &fileLimit) != 0) {
							child->failChildProcessModifier("file size limit", errno);
						}
					}
					// [[BPR-1252]]. Every gh request is a launch, so its sweep
					// must not cost one call per allowed descriptor.
					bpr::platform::unix_descriptor_sweep::bound();
				});
				process->setUnixProcessParameters(
					QProcess::UnixProcessFlag::CloseFileDescriptors
					| QProcess::UnixProcessFlag::DisableCoreDumps
					| QProcess::UnixProcessFlag::ResetSignalHandlers
					| QProcess::UnixProcessFlag::CreateNewSession);
#elif defined(Q_OS_WIN)
				// [[BPR-1114]]. The Windows counterpart of CloseFileDescriptors,
				// the platform target's launch restriction the Git adapter's
				// unguarded Git also takes ([[BPR-1115]]): every gh or Git this
				// runner starts inherits its three standard handles and nothing
				// else, where Qt alone would hand it every inheritable handle this
				// process holds. A launch whose list cannot be built is refused at
				// creation, so nothing starts, and the run fails as start_failure.
				process->setCreateProcessArgumentsModifier([&inheritance, aFailStandardHandleList](QProcess::CreateProcessArguments* const aArguments) {
					if (aFailStandardHandleList) {
						aArguments->startupInfo->hStdError = nullptr;
					}
					inheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles);
				});
#endif

				std::string standardOutput;
				standardOutput.reserve(std::min<std::size_t>(
					aRequest.StandardOutputLimit,
					64U * 1024U));
				std::size_t standardOutputBytes = 0;
				std::size_t standardErrorBytes  = 0;

				const auto discardOutput = [&standardOutput]() noexcept {
					std::fill(
						standardOutput.begin(),
						standardOutput.end(),
						'\0');
					standardOutput.clear();
				};
				const auto failed =
					[&](const github_cli_process_failure aFailure)
					-> process_execution {
					discardOutput();
					return failed_execution(
						process,
						*processGroupId,
						aRequest.ReapTimeout,
						aFailure);
				};
				const auto drainStreams =
					[&]() -> std::optional<github_cli_process_failure> {
					if (
						const auto failure = drain_channel(
							*process,
							process_stream::standard_output,
							aRequest.StandardOutputLimit,
							standardOutputBytes,
							aFileLimits.DiscardStandardOutput ? nullptr : &standardOutput);
						failure.has_value()) {
						return failure;
					}
					if (
						const auto failure = drain_channel(
							*process,
							process_stream::standard_error,
							aRequest.StandardErrorLimit,
							standardErrorBytes,
							nullptr);
						failure.has_value()) {
						return failure;
					}
					process->setReadChannel(QProcess::StandardOutput);
					return std::nullopt;
				};
				const auto terminalFailure =
					[&]() -> std::optional<github_cli_process_failure> {
					if (!directory_within_limits(aFileLimits)) {
						return github_cli_process_failure::standard_output_limit;
					}
					if (
						aShutdownRequested.load(std::memory_order_acquire)
						|| aStop.stop_requested()) {
						return github_cli_process_failure::cancelled;
					}
					if (clock::now() >= aDeadline) {
						return github_cli_process_failure::timeout;
					}
					return std::nullopt;
				};

				QObject::connect(
					process.get(),
					&QProcess::started,
					process.get(),
					[processPointer = process.get(),
					 processGroupId]() noexcept {
						*processGroupId = processPointer->processId();
					},
					Qt::DirectConnection);
				QObject::connect(
					process.get(),
					&QProcess::finished,
					process.get(),
					[processGroupId](
						const int,
						const QProcess::ExitStatus) noexcept {
						kill_process_group(*processGroupId);
					},
					Qt::DirectConnection);
				if (
					aShutdownRequested.load(std::memory_order_acquire)
					|| aStop.stop_requested()) {
					return failed(
						github_cli_process_failure::cancelled);
				}
				if (clock::now() >= aDeadline) {
					return failed(github_cli_process_failure::timeout);
				}
				process->start();
				*processGroupId = resolve_process_group(
					*process,
					*processGroupId);
				for (;;) {
					*processGroupId = resolve_process_group(
						*process,
						*processGroupId);
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}

					if (process->waitForStarted(wait_milliseconds(aDeadline))) {
						if (*processGroupId == 0) {
							*processGroupId = process->processId();
						}
						break;
					}
					if (process->state() == QProcess::NotRunning) {
						return failed(
							github_cli_process_failure::start_failure);
					}
				}
#if defined(Q_OS_WIN)
				// A list that could not be built never gets here: process creation
				// refused the start. This catches a child created without the
				// restriction all the same, which must not run.
				if (!inheritance.restricted()) {
					return failed(github_cli_process_failure::start_failure);
				}
#endif

				std::size_t inputOffset = 0;
				while (inputOffset < aRequest.StandardInput.size()) {
					const auto remaining = aRequest.StandardInput.size() - inputOffset;
					const auto accepted  = process->write(
						aRequest.StandardInput.data() + inputOffset,
						static_cast<qint64>(remaining));
					if (accepted < 0) {
						return failed(
							github_cli_process_failure::write_failure);
					}
					inputOffset += static_cast<std::size_t>(accepted);

					if (const auto failure = drainStreams(); failure.has_value()) {
						return failed(*failure);
					}
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}
					if (
						accepted == 0
						&& !process->waitForBytesWritten(
							wait_milliseconds(aDeadline))
						&& process->state() == QProcess::NotRunning) {
						return failed(
							github_cli_process_failure::write_failure);
					}
				}

				while (process->bytesToWrite() > 0) {
					static_cast<void>(
						process->waitForBytesWritten(
							wait_milliseconds(aDeadline)));
					if (const auto failure = drainStreams(); failure.has_value()) {
						return failed(*failure);
					}
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}
					if (process->state() == QProcess::NotRunning) {
						return failed(
							github_cli_process_failure::write_failure);
					}
				}
				process->closeWriteChannel();

				while (process->state() != QProcess::NotRunning) {
					process->setReadChannel(QProcess::StandardOutput);
					static_cast<void>(
						process->waitForReadyRead(
							wait_milliseconds(aDeadline)));
					if (const auto failure = drainStreams(); failure.has_value()) {
						return failed(*failure);
					}
					if (const auto failure = terminalFailure(); failure.has_value()) {
						return failed(*failure);
					}
				}

				if (const auto failure = drainStreams(); failure.has_value()) {
					return failed(*failure);
				}
				if (const auto failure = terminalFailure(); failure.has_value()) {
					return failed(*failure);
				}
				if (process->exitStatus() == QProcess::CrashExit) {
					return failed(
						github_cli_process_failure::process_crash);
				}
				if (!aRequest.PreserveCompletedOutput && process->exitCode() != 0) {
					return failed(
						github_cli_process_failure::nonzero_exit);
				}
				if (!aRequest.PreserveCompletedOutput && standardErrorBytes != 0) {
					return failed(
						github_cli_process_failure::unexpected_standard_error);
				}

				kill_process_group(*processGroupId);
				return {
					.Result = github_cli_process_success{
						.StandardOutput = std::move(standardOutput),
						.ExitCode       = process->exitCode(),
					},
					.AwaitingReap   = nullptr,
					.ProcessGroupId = 0,
				};
			} catch (...) {
				return failed_execution(
					process,
					*processGroupId,
					aRequest.ReapTimeout,
					github_cli_process_failure::start_failure);
			}
		}

		struct process_work_item
		{
			github_cli_process_request               Request;
			std::stop_token                          Stop;
			clock::time_point                        Deadline;
			std::mutex                               Mutex;
			std::condition_variable                  Completed;
			std::optional<github_cli_process_result> Result;
		};

		class process_worker final
		{
		public:
			explicit process_worker(
				std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam,
				github_cli_process_file_limits                       aFileLimits)
				: TestSeam{ std::move(aTestSeam) }
				, FileLimits{ std::move(aFileLimits) }
				, Worker{ [this] {
					worker_loop();
				} }
			{
			}

			process_worker(const process_worker&)                    = delete;
			auto operator=(const process_worker&) -> process_worker& = delete;
			~process_worker()
			{
				ShutdownRequested.store(true, std::memory_order_release);
				WorkAvailable.notify_all();
				SlotAvailable.notify_all();
				if (Worker.joinable()) {
					Worker.join();
				}
			}

			[[nodiscard]] auto submit(
				const github_cli_process_request& aRequest,
				const std::stop_token             aStop) -> github_cli_process_result
			{
				if (!valid_request_shape(aRequest)) {
					return std::unexpected{
						github_cli_process_failure::invalid_request
					};
				}
				if (aStop.stop_requested()) {
					return std::unexpected{
						github_cli_process_failure::cancelled
					};
				}

				std::shared_ptr<process_work_item> item;
				try {
					item           = std::make_shared<process_work_item>();
					item->Request  = aRequest;
					item->Stop     = aStop;
					item->Deadline = clock::now() + aRequest.Timeout;
				} catch (...) {
					return std::unexpected{
						github_cli_process_failure::start_failure
					};
				}

				{
					std::unique_lock lock{ Mutex };
					if (ShutdownRequested.load(std::memory_order_acquire)) {
						return std::unexpected{
							github_cli_process_failure::cancelled
						};
					}
					while (Busy) {
						if (ShutdownRequested.load(std::memory_order_acquire)) {
							return std::unexpected{
								github_cli_process_failure::cancelled
							};
						}
						if (Quarantined) {
							return std::unexpected{
								github_cli_process_failure::reap_failure
							};
						}
						if (aStop.stop_requested()) {
							return std::unexpected{
								github_cli_process_failure::cancelled
							};
						}
						if (clock::now() >= item->Deadline) {
							return std::unexpected{
								github_cli_process_failure::timeout
							};
						}
						SlotAvailable.wait_for(lock, wait_slice);
					}
					Busy    = true;
					Pending = item;
				}
				WorkAvailable.notify_one();

				std::unique_lock resultLock{ item->Mutex };
				const auto       completionDeadline = item->Deadline + aRequest.ReapTimeout + wait_slice;
				if (
					!item->Completed.wait_until(
						resultLock,
						completionDeadline,
						[&] {
							return item->Result.has_value();
						})) {
					{
						std::lock_guard lock{ Mutex };
						if (Busy) {
							Quarantined = true;
						}
					}
					SlotAvailable.notify_all();
					return std::unexpected{
						github_cli_process_failure::reap_failure
					};
				}
				return std::move(*item->Result);
			}

		private:
			auto publish(
				const std::shared_ptr<process_work_item>& aItem,
				github_cli_process_result                 aResult) -> void
			{
				{
					std::lock_guard lock{ aItem->Mutex };
					aItem->Result.emplace(std::move(aResult));
				}
				aItem->Completed.notify_one();
			}

			auto worker_loop() -> void
			{
				for (;;) {
					std::shared_ptr<process_work_item> item;
					{
						std::unique_lock lock{ Mutex };
						WorkAvailable.wait(
							lock,
							[&] {
								return Pending != nullptr
									|| ShutdownRequested.load(
										std::memory_order_acquire);
							});
						if (Pending == nullptr) {
							return;
						}
						item = std::move(Pending);
					}

					std::optional<process_execution> execution;
					try {
						if (TestSeam != nullptr) {
							TestSeam->before_execute();
						}
						const auto failStandardHandleList = TestSeam != nullptr
							&& TestSeam->fail_standard_handle_list();
						execution.emplace(execute_process(
							item->Request,
							item->Stop,
							ShutdownRequested,
							item->Deadline, FileLimits, failStandardHandleList));
					} catch (...) {
						publish(
							item,
							std::unexpected{
								github_cli_process_failure::start_failure });
						{
							std::lock_guard lock{ Mutex };
							Quarantined = false;
							Busy        = false;
						}
						SlotAvailable.notify_all();
						continue;
					}

					if (execution->AwaitingReap != nullptr) {
						{
							std::lock_guard lock{ Mutex };
							Quarantined = true;
						}
						SlotAvailable.notify_all();
					}
					publish(item, std::move(execution->Result));

					if (execution->AwaitingReap != nullptr) {
						finish_background_reap(
							*execution->AwaitingReap,
							execution->ProcessGroupId);
						execution->AwaitingReap.reset();
					}
					{
						std::lock_guard lock{ Mutex };
						Quarantined = false;
						Busy        = false;
					}
					SlotAvailable.notify_all();
				}
			}

			std::mutex                                           Mutex;
			std::condition_variable                              WorkAvailable;
			std::condition_variable                              SlotAvailable;
			std::shared_ptr<process_work_item>                   Pending;
			bool                                                 Busy              = false;
			bool                                                 Quarantined       = false;
			std::atomic_bool                                     ShutdownRequested = false;
			std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
			github_cli_process_file_limits                       FileLimits;
			std::thread                                          Worker;
		};

		class qprocess_github_cli_process_runner final
			: public github_cli_process_port
		{
		public:
			explicit qprocess_github_cli_process_runner(
				std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam,
				github_cli_process_file_limits                       aFileLimits)
				: TestSeam{ std::move(aTestSeam) }
				, FileLimits{ std::move(aFileLimits) }
			{
			}

			[[nodiscard]] auto run(
				const github_cli_process_request& aRequest,
				const std::stop_token             aStop) -> github_cli_process_result override
			{
				try {
					process_worker* worker = nullptr;
					{
						std::lock_guard lock{ WorkerMutex };
						if (Worker == nullptr) {
							Worker = std::make_unique<process_worker>(
								TestSeam, FileLimits);
						}
						worker = Worker.get();
					}
					return worker->submit(aRequest, aStop);
				} catch (...) {
					return std::unexpected{
						github_cli_process_failure::start_failure
					};
				}
			}

		private:
			std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
			github_cli_process_file_limits                       FileLimits;
			std::mutex                                           WorkerMutex;
			std::unique_ptr<process_worker>                      Worker;
		};

		[[nodiscard]] auto make_qprocess_github_cli_process_runner(
			std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam,
			github_cli_process_file_limits                       aFileLimits = { })
			-> std::unique_ptr<github_cli_process_port>
		{
			return std::make_unique<qprocess_github_cli_process_runner>(
				std::move(aTestSeam), std::move(aFileLimits));
		}
	}

	auto make_github_cli_process_runner()
		-> std::unique_ptr<github_cli_process_port>
	{
		return make_qprocess_github_cli_process_runner(nullptr);
	}

	// [[BPR-0807]]. RFC-BPR-0015's Git acquisition has its own factory so the
	// production factory above stays nullary, as the boundary gate requires,
	// and every file-bounded runner is visible at its call site.
	auto make_github_cli_git_acquisition_process_runner(github_cli_process_file_limits aFileLimits)
		-> std::unique_ptr<github_cli_process_port>
	{
		if (!aFileLimits.StandardOutputFile.empty()
			&& (aFileLimits.StandardOutputFile.parent_path() != aFileLimits.Directory
				|| std::filesystem::exists(aFileLimits.StandardOutputFile))) {
			return nullptr;
		}
		if (aFileLimits.MaximumFileBytes == 0U || aFileLimits.MaximumFileBytes > 32U * 1024U * 1024U
			|| !aFileLimits.Directory.is_absolute() || aFileLimits.MaximumDirectoryBytes == 0U
			|| aFileLimits.MaximumDirectoryEntries == 0U || aFileLimits.MaximumDirectoryEntries > 256U) {
			return nullptr;
		}
		return make_qprocess_github_cli_process_runner(nullptr, std::move(aFileLimits));
	}

	auto make_github_cli_process_runner_for_test(
		std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam)
		-> std::unique_ptr<github_cli_process_port>
	{
		return make_qprocess_github_cli_process_runner(
			std::move(aTestSeam));
	}
}
