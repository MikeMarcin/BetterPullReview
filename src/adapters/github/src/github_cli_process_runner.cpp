#include "github_cli_process_runner.hpp"
#include "bpr/adapters/process_support/process_supervision.hpp"

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
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#if defined(Q_OS_UNIX)
	#include <cerrno>
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
		using clock = process_support::supervision_clock;
		using process_support::kill_process_group;
		using process_support::process_stream;
		using process_support::resolve_process_group;
		using process_support::wait_milliseconds;

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

		constexpr std::array<std::string_view, 4U> credential_environment_names{ "GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN" };

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
				if (std::ranges::any_of(credential_environment_names, [&entry](std::string_view aName) { return equal_environment_names(entry.Name, aName); })) {
					return false;
				}
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

		// Withhold a possible credential prefix across read chunks. No complete
		// credential reaches the generic response string, even when a child echoes it.

		class credential_output_filter final
		{
		public:
			explicit credential_output_filter(cli_transient_credential aCredential)
				: Credential{ std::move(aCredential) }
			{
				const auto secret = Credential->view();
				for (std::size_t index = 1U, matched = 0U; index < secret.size(); ++index) {
					while (matched != 0U && secret[index] != secret[matched]) {
						matched = Prefix[matched - 1U];
					}
					if (secret[index] == secret[matched]) {
						++matched;
					}
					Prefix[index] = matched;
				}
			}
			~credential_output_filter()
			{
				volatile std::size_t* cursor = Prefix.data();
				for (std::size_t index = 0U; index < Prefix.size(); ++index) {
					cursor[index] = 0U;
				}
			}
			[[nodiscard]] auto append(std::string_view aBytes, std::string* aOutput) -> bool
			{
				const auto secret = Credential->view();
				for (char byte : aBytes) {
					while (Matched != 0U && secret[Matched] != byte) {
						const auto fallback = Prefix[Matched - 1U];
						if (aOutput) {
							aOutput->append(secret.substr(0U, Matched - fallback));
						}
						Matched = fallback;
					}
					if (secret[Matched] == byte) {
						if (++Matched == secret.size()) {
							return false;
						}
					} else if (aOutput) {
						aOutput->push_back(byte);
					}
				}
				return true;
			}
			void finish(std::string* aOutput)
			{
				if (aOutput) {
					aOutput->append(Credential->view().substr(0U, Matched));
				}
				Matched = 0U;
			}

		private:
			cli_transient_credential       Credential;
			std::array<std::size_t, 8192U> Prefix{ };
			std::size_t                    Matched = 0U;
		};

		[[nodiscard]] auto drain_channel(
			QProcess&            aProcess,
			const process_stream aStream,
			const std::size_t    aLimit,
			std::size_t&         aConsumed,
			std::string*         aRetainedOutput,
			cli_sensitive_bytes* aSensitiveOutput = nullptr, credential_output_filter* aFilter = nullptr)
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
			struct wipe_chunk
			{
				std::array<char, 16U * 1024U>& Bytes;
				~wipe_chunk()
				{
					volatile char* cursor = Bytes.data();
					for (std::size_t i = 0; i < Bytes.size(); ++i) {
						cursor[i] = 0;
					}
				}
			} wipe{ buffer };
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
				if (aSensitiveOutput != nullptr && !aSensitiveOutput->append({ buffer.data(), byteCount })) {
					return failure;
				}
				if (aFilter != nullptr) {
					if (!aFilter->append({ buffer.data(), byteCount }, aRetainedOutput)) {
						return failure;
					}
				} else if (aRetainedOutput != nullptr) {
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

		/// \brief How this runner kills a launch's tree.
		/// \details The test seam may stand in for a child that survives SIGKILL
		/// ([[BPR-1436]]); production runners have no seam.
		[[nodiscard]] auto tree_kill_for(
			const std::shared_ptr<github_cli_process_runner_test_seam>& aTestSeam)
			-> process_support::process_tree_kill
		{
			if (aTestSeam != nullptr && aTestSeam->child_survives_kill()) {
				return [](QProcess&, qint64) { };
			}
			return &process_support::kill_process_tree;
		}

		[[nodiscard]] auto failed_execution(
			std::unique_ptr<QProcess>&                aProcess,
			const qint64                              aKnownProcessGroupId,
			const std::chrono::milliseconds           aReapTimeout,
			const github_cli_process_failure          aFailure,
			const process_support::process_tree_kill& aKillTree = process_support::process_tree_kill{ &process_support::kill_process_tree }) -> process_execution
		{
			const auto processGroupId = resolve_process_group(*aProcess, aKnownProcessGroupId);
			if (
				process_support::kill_process_group_and_reap(
					*aProcess,
					processGroupId,
					aReapTimeout,
					aKillTree)) {
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
			const github_cli_process_request&           aRequest,
			const std::stop_token                       aStop,
			const std::atomic_bool&                     aShutdownRequested,
			const clock::time_point                     aDeadline,
			const github_cli_process_file_limits&       aFileLimits,
			[[maybe_unused]] const bool                 aFailStandardHandleList,
			const std::shared_ptr<cli_sensitive_bytes>& aCapture,
			const cli_transient_credential& aCredential, cli_credential_environment aCredentialEnvironment,
			const std::shared_ptr<github_cli_process_runner_test_seam>& aTestSeam) -> process_execution
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
			const auto killTree       = tree_kill_for(aTestSeam);
			try {
				if ((aCapture || aCredential) && (!aFileLimits.StandardOutputFile.empty() || (aRequest.PreserveCompletedOutput && aCapture))) {
					return failed_execution(process, 0, aRequest.ReapTimeout, github_cli_process_failure::invalid_request);
				}
				if (aCredential) {
					const auto secret = aCredential->view();
					const auto name   = QString::fromLatin1(credential_environment_names[aCredentialEnvironment == cli_credential_environment::github ? 0U : 2U].data());
					if (secret.empty() || secret.size() > 8192U || !std::ranges::all_of(secret, [](unsigned char aByte) { return aByte > 32U && aByte < 127U; })
						|| secret.size() > github_cli_process_max_environment_value_bytes
						|| aRequest.Environment.size() >= github_cli_process_max_environment_count) {
						return failed_execution(process, 0, aRequest.ReapTimeout, github_cli_process_failure::invalid_request);
					}
					std::size_t environmentBytes = secret.size() + static_cast<std::size_t>(name.size());
					for (const auto& entry : aRequest.Environment) {
						environmentBytes += entry.Name.size() + entry.Value.size();
					}
					if (environmentBytes > github_cli_process_max_environment_bytes || aRequest.StandardInput.find(secret) != std::string::npos
						|| std::ranges::any_of(aRequest.Arguments, [secret](const auto& aArgument) { return aArgument.find(secret) != std::string::npos; })) {
						return failed_execution(process, 0, aRequest.ReapTimeout, github_cli_process_failure::invalid_request);
					}
					// Only Qt/OS launch storage receives this copy; the generic request stays token-free.
					auto value = QString::fromLatin1(secret.data(), static_cast<qsizetype>(secret.size()));
					validated.Environment.insert(name, value);
					value.fill(QChar{ 0 });
				}
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
				std::optional<credential_output_filter> outputFilter;
				if (aCredential) {
					outputFilter.emplace(aCredential);
				}
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
					if (aRequest.Observation) {
						aRequest.Observation->StandardOutputBytes.store(standardOutputBytes);
						aRequest.Observation->StandardErrorBytes.store(standardErrorBytes);
						if (process->state() == QProcess::NotRunning && *processGroupId != 0) {
							aRequest.Observation->ExitCode.store(process->exitCode());
						}
					}
					discardOutput();
					return failed_execution(
						process,
						*processGroupId,
						aRequest.ReapTimeout,
						aFailure,
						killTree);
				};
				const auto drainStreams =
					[&]() -> std::optional<github_cli_process_failure> {
					if (
						const auto failure = drain_channel(
							*process,
							process_stream::standard_output,
							aRequest.StandardOutputLimit,
							standardOutputBytes,
							aCapture || aFileLimits.DiscardStandardOutput ? nullptr : &standardOutput, aCapture.get(), outputFilter ? &*outputFilter : nullptr);
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

				if (aRequest.Insights) {
					core::insights_event event;
					event.Kind     = core::insights_event_kind::process;
					event.Launched = true;
					aRequest.Insights->observe(event);
					if (aRequest.InsightsResource) {
						aRequest.Insights->request(*aRequest.InsightsResource, aRequest.InsightsCategory);
					}
				}

				if (aTestSeam != nullptr) {
					aTestSeam->after_started();
				}
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

				if (aRequest.Observation) {
					aRequest.Observation->StandardOutputBytes.store(standardOutputBytes);
					aRequest.Observation->StandardErrorBytes.store(standardErrorBytes);
					aRequest.Observation->ExitCode.store(process->exitCode());
				}
				if (outputFilter) {
					outputFilter->finish(aFileLimits.DiscardStandardOutput ? nullptr : &standardOutput);
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
					github_cli_process_failure::start_failure,
					killTree);
			}
		}

		struct process_work_item
			: process_support::process_work_state<github_cli_process_result>
		{
			github_cli_process_request           Request;
			std::shared_ptr<cli_sensitive_bytes> Capture;
			cli_transient_credential             Credential;
			cli_credential_environment           CredentialEnvironment = cli_credential_environment::github;
		};

		/// \brief What the shared lane runs for this runner ([[BPR-1436]]).
		struct process_lane_traits
		{
			using item      = process_work_item;
			using result    = github_cli_process_result;
			using execution = process_execution;

			[[nodiscard]] static auto failure(
				const process_support::lane_failure aFailure) -> github_cli_process_result
			{
				switch (aFailure) {
				case process_support::lane_failure::cancelled:
					return std::unexpected{ github_cli_process_failure::cancelled };
				case process_support::lane_failure::timeout:
					return std::unexpected{ github_cli_process_failure::timeout };
				case process_support::lane_failure::reap_failure:
					return std::unexpected{ github_cli_process_failure::reap_failure };
				case process_support::lane_failure::start_failure:
					break;
				}
				return std::unexpected{ github_cli_process_failure::start_failure };
			}

			[[nodiscard]] auto execute(
				process_work_item&      aItem,
				const std::atomic_bool& aShutdownRequested) -> process_execution
			{
				if (TestSeam != nullptr) {
					TestSeam->before_execute();
				}
				const auto failStandardHandleList = TestSeam != nullptr
					&& TestSeam->fail_standard_handle_list();
				const auto started   = clock::now();
				auto       execution = execute_process(
					aItem.Request,
					aItem.Stop,
					aShutdownRequested,
					aItem.Deadline, FileLimits, failStandardHandleList, aItem.Capture, aItem.Credential, aItem.CredentialEnvironment, TestSeam);
				if (aItem.Request.Insights) {
					core::insights_event event;
					event.Kind                 = core::insights_event_kind::process;
					event.DurationMicroseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - started).count());
					if (execution.Result && execution.Result->ExitCode != 0) {
						event.ProcessFailure = core::insights_process_failure::nonzero_exit;
					}
					if (!execution.Result) {
						using failure  = github_cli_process_failure;
						using observed = core::insights_process_failure;
						switch (execution.Result.error()) {
						case failure::timeout:               event.ProcessFailure = observed::timeout; break;
						case failure::cancelled:             event.ProcessFailure = observed::cancelled; break;
						case failure::standard_output_limit:
						case failure::standard_error_limit:  event.ProcessFailure = observed::output_limit; break;
						case failure::process_crash:         event.ProcessFailure = observed::crash; break;
						case failure::nonzero_exit:          event.ProcessFailure = observed::nonzero_exit; break;
						case failure::reap_failure:          event.ProcessFailure = observed::reap; break;
						case failure::start_failure:         event.ProcessFailure = observed::start; break;
						default:                             event.ProcessFailure = observed::other; break;
						}
					}
					aItem.Request.Insights->observe(event);
				}
				return execution;
			}

			/// \brief The bounded last attempt on a child the launch could not reap.
			/// \details Given the longest reap a request may ask for. Until
			/// [[BPR-1436]] this loop had no deadline, so one child that never
			/// exited held the worker, and destroying the runner never returned.
			[[nodiscard]] auto finish_reap(process_execution& aExecution) const -> bool
			{
				return process_support::finish_background_reap(
					*aExecution.AwaitingReap,
					aExecution.ProcessGroupId,
					github_cli_process_max_reap_timeout,
					tree_kill_for(TestSeam));
			}

			static void observe(process_work_item& aItem, const process_support::lane_observation aObservation) noexcept
			{
				if (aItem.Request.Insights) {
					core::insights_event event;
					event.Kind        = core::insights_event_kind::process;
					event.Quarantined = aObservation == process_support::lane_observation::quarantined;
					event.Abandoned   = aObservation == process_support::lane_observation::abandoned;
					aItem.Request.Insights->observe(event);
				}
			}

			std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
			github_cli_process_file_limits                       FileLimits;
		};

		class process_worker final
		{
		public:
			explicit process_worker(
				std::shared_ptr<github_cli_process_runner_test_seam> aTestSeam,
				github_cli_process_file_limits                       aFileLimits)
				: Lane{ process_lane_traits{
					  .TestSeam   = std::move(aTestSeam),
					  .FileLimits = std::move(aFileLimits),
				  } }
			{
			}

			[[nodiscard]] auto submit(
				const github_cli_process_request&    aRequest,
				const std::stop_token                aStop,
				std::shared_ptr<cli_sensitive_bytes> aCapture = { }, cli_transient_credential aCredential = { },
				cli_credential_environment aCredentialEnvironment = cli_credential_environment::github) -> github_cli_process_result
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
					item                        = std::make_shared<process_work_item>();
					item->Request               = aRequest;
					item->Capture               = std::move(aCapture);
					item->Credential            = std::move(aCredential);
					item->CredentialEnvironment = aCredentialEnvironment;
					item->Stop                  = aStop;
					item->Deadline              = clock::now() + aRequest.Timeout;
				} catch (...) {
					return std::unexpected{
						github_cli_process_failure::start_failure
					};
				}
				return Lane.submit(item, aRequest.ReapTimeout);
			}

		private:
			process_support::serialized_process_lane<process_lane_traits> Lane;
		};

		class qprocess_github_cli_process_runner final
			: public github_cli_process_port
		{
		public:
			void set_insights_channel(std::shared_ptr<github_insights_channel> aChannel) override
			{
				std::lock_guard lock{ WorkerMutex };
				Insights = std::move(aChannel);
			}

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
				return dispatch(aRequest, aStop);
			}

			[[nodiscard]] auto acquire_sensitive(const github_cli_process_request& aRequest, std::stop_token aStop, std::shared_ptr<const void> aLifetime) -> cli_sensitive_result override
			{
				if (aRequest.StandardOutputLimit != cli_sensitive_bytes::capacity || aRequest.PreserveCompletedOutput) {
					return std::unexpected{ github_cli_process_failure::invalid_request };
				}
				struct capture_owner
				{
					cli_sensitive_bytes         Bytes;
					std::shared_ptr<const void> Lifetime;
				};
				auto owner         = std::make_shared<capture_owner>();
				owner->Lifetime    = std::move(aLifetime);
				auto       capture = std::shared_ptr<cli_sensitive_bytes>{ owner, &owner->Bytes };
				const auto answer  = dispatch(aRequest, aStop, capture);
				if (!answer) {
					return std::unexpected{ answer.error() };
				}
				if (!capture->finish_credential()) {
					return std::unexpected{ github_cli_process_failure::invalid_request };
				}
				return cli_transient_credential{ std::move(capture) };
			}
			[[nodiscard]] auto run_credentialed(const github_cli_process_request& aRequest, cli_transient_credential aCredential,
												cli_credential_environment aEnvironment, std::stop_token aStop) -> github_cli_process_result override
			{
				if (!aCredential || (aEnvironment != cli_credential_environment::github && aEnvironment != cli_credential_environment::enterprise)) {
					return std::unexpected{ github_cli_process_failure::invalid_request };
				}
				return dispatch(aRequest, aStop, { }, std::move(aCredential), aEnvironment);
			}

		private:
			[[nodiscard]] auto dispatch(github_cli_process_request aRequest, std::stop_token aStop,
										std::shared_ptr<cli_sensitive_bytes> aCapture = { }, cli_transient_credential aCredential = { },
										cli_credential_environment aEnvironment = cli_credential_environment::github) -> github_cli_process_result
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
						if (!aRequest.Insights) {
							aRequest.Insights = Insights;
						}
					}
					return worker->submit(aRequest, aStop, std::move(aCapture), std::move(aCredential), aEnvironment);
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
			std::shared_ptr<github_insights_channel>             Insights;
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
