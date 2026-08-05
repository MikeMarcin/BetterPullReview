#include "github_cli_process_runner.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
	#include <csignal>
	#include <fcntl.h>
	#include <sys/resource.h>
	#include <sys/types.h>
	#include <unistd.h>

extern char** environ;
#endif

#if defined(_WIN32)
	#include <windows.h>
#endif

namespace {
	using namespace std::chrono_literals;
	using namespace bpr::adapters::github::detail;

	static_assert(!std::is_copy_constructible_v<github_cli_process_port>);
	static_assert(!std::is_move_constructible_v<github_cli_process_port>);
	static_assert(
		std::is_same_v<
			github_cli_process_result::error_type,
			github_cli_process_failure>);

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto utf8(const QString& aValue) -> std::string
	{
		const auto bytes = aValue.toUtf8();
		return {
			bytes.constData(),
			static_cast<std::size_t>(bytes.size()),
		};
	}

#if defined(Q_OS_MACOS)
	[[nodiscard]] auto macos_text_encoding_environment() -> std::string
	{
		return "0x"
			+ utf8(
				   QString::number(
					   static_cast<qulonglong>(::geteuid()),
					   16)
					   .toUpper())
			+ ":0x0:0x0";
	}
#endif

	[[nodiscard]] auto parse_count(const QString& aValue) -> std::size_t
	{
		bool       valid  = false;
		const auto parsed = aValue.toULongLong(&valid);
		if (
			!valid
			|| parsed
				> static_cast<qulonglong>(
					std::numeric_limits<std::size_t>::max())) {
			return 0;
		}
		return static_cast<std::size_t>(parsed);
	}

	[[nodiscard]] auto exact_environment_matches(
		const QString&     aUnknownName,
		const QStringList& aArguments) -> bool
	{
#if defined(_WIN32)
		const auto  environment = QProcessEnvironment::systemEnvironment();
		QStringList expected_names{
			QStringLiteral("BPR_ALLOWED_PROCESS_FACT"),
			QStringLiteral("PATH"),
			QStringLiteral("SystemRoot"),
		};
		auto actual_names = environment.keys();
		actual_names.sort(Qt::CaseInsensitive);
		expected_names.sort(Qt::CaseInsensitive);
		return actual_names == expected_names
			&& environment.value(
				   QStringLiteral("BPR_ALLOWED_PROCESS_FACT"))
			== QStringLiteral("fixed")
			&& environment.value(QStringLiteral("PATH")).isEmpty()
			&& environment.value(QStringLiteral("SystemRoot"))
			== aArguments.at(7)
			&& !environment.contains(aUnknownName);
#else
		static_cast<void>(aArguments);
		std::vector<std::pair<std::string, std::string>> actual;
		for (auto current = ::environ; current != nullptr && *current != nullptr;
			 ++current) {
			const std::string_view entry{ *current };
			const auto             separator = entry.find('=');
			if (separator == std::string_view::npos) {
				return false;
			}
			actual.emplace_back(
				entry.substr(0U, separator),
				entry.substr(separator + 1U));
		}

		std::vector<std::pair<std::string, std::string>> expected{
			{ "BPR_ALLOWED_PROCESS_FACT", "fixed" },
		};
	#if defined(Q_OS_MACOS)
		const auto platformEntry = std::ranges::find(
			actual,
			std::pair{
				std::string{ "__CF_USER_TEXT_ENCODING" },
				macos_text_encoding_environment() });
		if (platformEntry == actual.end()) {
			return false;
		}
		actual.erase(platformEntry);
	#elif defined(Q_OS_LINUX)
		expected.emplace_back("LC_ALL", "C.UTF-8");
	#endif
		std::ranges::sort(actual);
		std::ranges::sort(expected);
		return actual == expected
			&& std::ranges::none_of(
				   actual,
				   [&aUnknownName](const auto& aEntry) {
					   return aEntry.first == utf8(aUnknownName);
				   });
#endif
	}

	[[nodiscard]] auto helper_main(const QStringList& aArguments) -> int
	{
		if (aArguments.size() < 3) {
			return 91;
		}

		const auto& mode = aArguments.at(2);
		if (mode == QStringLiteral("exact")) {
#if defined(_WIN32)
			constexpr auto exactArgumentCount = 8;
#else
			constexpr auto exactArgumentCount = 7;
#endif
			if (aArguments.size() != exactArgumentCount) {
				return 92;
			}
			if (
				!exact_environment_matches(aArguments.at(5), aArguments)
				|| QDir::current().canonicalPath() != aArguments.at(6)) {
				return 93;
			}

			const std::string input{
				std::istreambuf_iterator<char>{ std::cin },
				std::istreambuf_iterator<char>{ },
			};
			const std::string expected{ "body\0bytes", 10 };
			if (
				aArguments.at(3) != QStringLiteral("alpha")
				|| aArguments.at(4) != QStringLiteral("beta")
				|| input != expected) {
				return 94;
			}
			std::cout << "ok";
			return 0;
		}

		if (
			mode == QStringLiteral("stdout")
			|| mode == QStringLiteral("stderr")
			|| mode == QStringLiteral("both")) {
			if (aArguments.size() != 4) {
				return 95;
			}
			const auto        count = parse_count(aArguments.at(3));
			const std::string bytes(count, 'x');
			if (mode == QStringLiteral("stdout")) {
				std::cout.write(
					bytes.data(),
					static_cast<std::streamsize>(bytes.size()));
				return 0;
			}
			if (mode == QStringLiteral("stderr")) {
				std::cerr.write(
					bytes.data(),
					static_cast<std::streamsize>(bytes.size()));
				return 0;
			}

			std::thread outputWriter{
				[&bytes] {
					std::cout.write(
						bytes.data(),
						static_cast<std::streamsize>(bytes.size()));
					std::cout.flush();
				}
			};
			std::thread errorWriter{
				[&bytes] {
					std::cerr.write(
						bytes.data(),
						static_cast<std::streamsize>(bytes.size()));
					std::cerr.flush();
				}
			};
			outputWriter.join();
			errorWriter.join();
			return 0;
		}

		if (mode == QStringLiteral("hang")) {
			std::this_thread::sleep_for(10s);
			return 0;
		}
		if (mode == QStringLiteral("nonzero")) {
			std::cout << "discard-me";
			return 7;
		}
		if (mode == QStringLiteral("touch")) {
			if (aArguments.size() < 4) {
				return 102;
			}
			QFile marker{ aArguments.at(3) };
			if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				return 103;
			}
			return marker.write("started", 7) == 7 ? 0 : 104;
		}
		if (mode == QStringLiteral("crash")) {
			std::cout << "discard-me";
			std::cout.flush();
#if defined(Q_OS_UNIX)
			static_cast<void>(::kill(::getpid(), SIGKILL));
			return 97;
#elif defined(_WIN32)
			static_cast<void>(
				TerminateProcess(GetCurrentProcess(), 137U));
			return 97;
#else
			return 97;
#endif
		}
#if defined(Q_OS_UNIX)
		if (mode == QStringLiteral("unix-hardening")) {
			if (aArguments.size() != 4) {
				return 105;
			}
			bool       validDescriptor     = false;
			const auto inheritedDescriptor = aArguments.at(3).toInt(&validDescriptor);
			errno                          = 0;
			if (
				!validDescriptor
				|| ::fcntl(inheritedDescriptor, F_GETFD) != -1
				|| errno != EBADF) {
				return 106;
			}

			struct sigaction signalDisposition{ };
			if (
				::sigaction(SIGUSR1, nullptr, &signalDisposition) != 0
				|| signalDisposition.sa_handler != SIG_DFL) {
				return 107;
			}

			struct rlimit coreLimit{ };
			if (
				::getrlimit(RLIMIT_CORE, &coreLimit) != 0
				|| coreLimit.rlim_cur != 0) {
				return 108;
			}
			if (
				::getsid(0) != ::getpid()
				|| ::getpgrp() != ::getpid()) {
				return 109;
			}
			std::cout << "hardened";
			return 0;
		}

		if (
			mode == QStringLiteral("descendant")
			|| mode == QStringLiteral("success-descendant")
			|| mode == QStringLiteral("failure-descendant")
			|| mode == QStringLiteral("escaped-group-descendant")) {
			if (aArguments.size() != 4) {
				return 98;
			}
			const bool cleanSuccess = mode == QStringLiteral("success-descendant");
			const bool fixedFailure = mode == QStringLiteral("failure-descendant");
			const bool escapedGroup = mode == QStringLiteral("escaped-group-descendant");
			const auto descendant   = ::fork();
			if (descendant < 0) {
				return 99;
			}
			if (descendant == 0) {
				if (escapedGroup && ::setpgid(0, 0) != 0) {
					return 110;
				}
				if (cleanSuccess || fixedFailure || escapedGroup) {
					static_cast<void>(::close(STDIN_FILENO));
					static_cast<void>(::close(STDOUT_FILENO));
					static_cast<void>(::close(STDERR_FILENO));
				}
				for (;;) {
					static_cast<void>(::pause());
				}
			}

			if (
				escapedGroup
				&& ::setpgid(descendant, descendant) != 0) {
				static_cast<void>(::kill(descendant, SIGKILL));
				return 111;
			}
			QFile pidFile{ aArguments.at(3) };
			if (!pidFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
				static_cast<void>(::kill(descendant, SIGKILL));
				return 100;
			}
			const auto bytes = QByteArray::number(
				static_cast<qlonglong>(descendant));
			if (pidFile.write(bytes) != bytes.size() || !pidFile.flush()) {
				static_cast<void>(::kill(descendant, SIGKILL));
				return 101;
			}
			pidFile.close();
			if (cleanSuccess || escapedGroup) {
				std::cout << "clean";
				return 0;
			}
			if (fixedFailure) {
				std::cout << "discard-me";
				return 7;
			}
			for (;;) {
				static_cast<void>(::pause());
			}
		}
#endif
		return 96;
	}

	[[nodiscard]] auto canonical_application_path() -> std::string
	{
		return utf8(
			QFileInfo{ QCoreApplication::applicationFilePath() }
				.canonicalFilePath());
	}

	[[nodiscard]] auto canonical_directory(const QTemporaryDir& aDirectory)
		-> std::string
	{
		return utf8(QFileInfo{ aDirectory.path() }.canonicalFilePath());
	}

#if defined(_WIN32)
	[[nodiscard]] auto windows_system_root() -> std::string
	{
		wchar_t    buffer[MAX_PATH + 1]{ };
		const auto length = GetWindowsDirectoryW(buffer, MAX_PATH + 1);
		expect(
			length != 0U && length < MAX_PATH + 1U,
			"Windows system root is obtained without ambient environment");
		return utf8(
			QFileInfo{
				QString::fromWCharArray(
					buffer,
					static_cast<qsizetype>(length)) }
				.canonicalFilePath());
	}
#endif

	[[nodiscard]] auto base_request(
		const QTemporaryDir&     aDirectory,
		std::vector<std::string> aArguments) -> github_cli_process_request
	{
		std::vector<github_cli_process_environment_entry> environment{
			{
				.Name  = "BPR_ALLOWED_PROCESS_FACT",
				.Value = "fixed",
			},
		};
#if defined(Q_OS_LINUX)
		environment.push_back({
			.Name  = "LC_ALL",
			.Value = "C.UTF-8",
		});
#elif defined(_WIN32)
		environment.push_back({
			.Name  = "PATH",
			.Value = "",
		});
		environment.push_back({
			.Name  = "SystemRoot",
			.Value = windows_system_root(),
		});
#endif
		return {
			.Executable          = canonical_application_path(),
			.Arguments           = std::move(aArguments),
			.Environment         = std::move(environment),
			.WorkingDirectory    = canonical_directory(aDirectory),
			.StandardInput       = { },
			.StandardInputLimit  = 64U,
			.StandardOutputLimit = 64U,
			.StandardErrorLimit  = 64U,
			.Timeout             = 2s,
			.ReapTimeout         = 1s,
		};
	}

	auto expect_failure(
		github_cli_process_port&          aRunner,
		const github_cli_process_request& aRequest,
		const github_cli_process_failure  aExpected,
		const std::string_view            aMessage) -> void
	{
		const auto result = aRunner.run(aRequest, { });
		expect(!result.has_value() && result.error() == aExpected, aMessage);
	}

	auto test_exact_process_boundary() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "neutral process directory is available");

		const auto suffix = std::to_string(
			std::chrono::steady_clock::now()
				.time_since_epoch()
				.count());
		const auto unknownName = QByteArray::fromStdString(
			"BPR_UNKNOWN_PROCESS_CANARY_" + suffix);
		const auto unknownValue = QByteArray::fromStdString("runtime-only-" + suffix);
		expect(
			qputenv(unknownName.constData(), unknownValue),
			"unknown parent canary is installed");

		auto request = base_request(
			directory,
			{
				"--bpr-helper",
				"exact",
				"alpha",
				"beta",
				unknownName.toStdString(),
				canonical_directory(directory),
#if defined(_WIN32)
				windows_system_root(),
#endif
			});
		request.StandardInput       = std::string{ "body\0bytes", 10 };
		request.StandardInputLimit  = request.StandardInput.size();
		request.StandardOutputLimit = 2U;

		const auto runner = make_github_cli_process_runner();
		const auto result = runner->run(request, { });
		static_cast<void>(qunsetenv(unknownName.constData()));

		expect(
			result.has_value() && result->StandardOutput == "ok",
			"argv, stdin, cwd, and constructed child environment are exact");

		request.StandardInput.push_back('!');
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::invalid_request,
			"stdin N+1 is rejected before launch");
	}

	auto test_bare_repository_working_directory_is_rejected() -> void
	{
		QTemporaryDir repository;
		expect(repository.isValid(), "bare repository fixture root is available");
		QDir root{ repository.path() };
		expect(
			root.mkdir(QStringLiteral("objects"))
				&& root.mkdir(QStringLiteral("refs"))
				&& root.mkdir(QStringLiteral("nested")),
			"bare repository fixture directories are created");

		QFile head{ root.filePath(QStringLiteral("HEAD")) };
		expect(head.open(QIODevice::WriteOnly), "bare repository HEAD is created");
		expect(
			head.write("ref: refs/heads/main\n", 21) == 21,
			"bare repository HEAD is populated");
		head.close();

		auto request             = base_request(repository, { "--bpr-helper", "stdout", "2" });
		request.WorkingDirectory = utf8(
			QFileInfo{ root.filePath(QStringLiteral("nested")) }
				.canonicalFilePath());
		const auto runner = make_github_cli_process_runner();
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::invalid_request,
			"working directory below a bare repository is rejected");
	}

	auto test_shape_limits_reject_without_launch() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "shape test directory is available");
		const auto runner = make_github_cli_process_runner();
		const auto marker = directory.filePath(QStringLiteral("shape-limit.marker"));

		const auto touchRequest = [&] {
			return base_request(
				directory,
				{
					"--bpr-helper",
					"touch",
					utf8(marker),
				});
		};
		const auto expectRejected =
			[&](const github_cli_process_request& aRequest,
				const std::string_view            aMessage) {
				static_cast<void>(QFile::remove(marker));
				expect_failure(
					*runner,
					aRequest,
					github_cli_process_failure::invalid_request,
					aMessage);
				expect(
					!QFileInfo{ marker }.exists(),
					"shape-invalid request never launches its child");
			};

		auto request = touchRequest();
		request.Executable.assign(
			github_cli_process_max_path_bytes + 1U,
			'x');
		expectRejected(request, "executable path N+1 is rejected");

		request = touchRequest();
		request.Executable.push_back('\0');
		expectRejected(request, "executable path NUL is rejected");

		request = touchRequest();
		request.WorkingDirectory.assign(
			github_cli_process_max_path_bytes + 1U,
			'x');
		expectRejected(request, "working directory path N+1 is rejected");

		request = touchRequest();
		request.WorkingDirectory.push_back('\0');
		expectRejected(request, "working directory path NUL is rejected");

		request = touchRequest();
		request.Arguments.emplace_back(
			github_cli_process_max_argument_bytes + 1U,
			'x');
		expectRejected(request, "argument size N+1 is rejected");

		request = touchRequest();
		while (
			request.Arguments.size()
			<= github_cli_process_max_argument_count) {
			request.Arguments.emplace_back("x");
		}
		expectRejected(request, "argument count N+1 is rejected");

		request = touchRequest();
		for (std::size_t index = 0; index < 3U; ++index) {
			request.Arguments.emplace_back(
				github_cli_process_max_argument_bytes,
				'x');
		}
		expectRejected(request, "aggregate argument bytes N+1 are rejected");

		request = touchRequest();
		request.Arguments.emplace_back("x\0y", 3U);
		expectRejected(request, "argument NUL is rejected");

		request = touchRequest();
		while (
			request.Environment.size()
			<= github_cli_process_max_environment_count) {
			request.Environment.push_back({
				.Name  = "BPR_SHAPE_" + std::to_string(request.Environment.size()),
				.Value = "x",
			});
		}
		expectRejected(request, "environment count N+1 is rejected");

		request = touchRequest();
		request.Environment.push_back({
			.Name = std::string(
				github_cli_process_max_environment_name_bytes + 1U,
				'A'),
			.Value = "x",
		});
		expectRejected(request, "environment name size N+1 is rejected");

		request = touchRequest();
		request.Environment.push_back({
			.Name  = "BPR_SHAPE_VALUE",
			.Value = std::string(
				github_cli_process_max_environment_value_bytes + 1U,
				'x'),
		});
		expectRejected(request, "environment value size N+1 is rejected");

		request = touchRequest();
		for (std::size_t index = 0; index < 3U; ++index) {
			request.Environment.push_back({
				.Name  = "BPR_SHAPE_AGGREGATE_" + std::to_string(index),
				.Value = std::string(
					github_cli_process_max_environment_value_bytes,
					'x'),
			});
		}
		expectRejected(request, "aggregate environment bytes N+1 are rejected");

		request = touchRequest();
		request.Environment.push_back(
			request.Environment.front());
		expectRejected(request, "duplicate environment names are rejected");

		request                    = touchRequest();
		request.StandardInputLimit = github_cli_process_max_standard_input_bytes + 1U;
		expectRejected(request, "stdin limit N+1 is rejected");

		request                    = touchRequest();
		request.StandardInputLimit = github_cli_process_max_standard_input_bytes;
		request.StandardInput.assign(
			github_cli_process_max_standard_input_bytes + 1U,
			'x');
		expectRejected(request, "stdin payload N+1 is rejected");

		request                     = touchRequest();
		request.StandardOutputLimit = github_cli_process_max_standard_output_bytes + 1U;
		expectRejected(request, "stdout limit N+1 is rejected");

		request                    = touchRequest();
		request.StandardErrorLimit = github_cli_process_max_standard_error_bytes + 1U;
		expectRejected(request, "stderr limit N+1 is rejected");

		request         = touchRequest();
		request.Timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
							  github_cli_process_max_timeout)
			+ 1ms;
		expectRejected(request, "process timeout N+1 is rejected");

		request             = touchRequest();
		request.ReapTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(
								  github_cli_process_max_reap_timeout)
			+ 1ms;
		expectRejected(request, "reap timeout N+1 is rejected");
	}

	auto test_stream_limits_and_redaction() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "stream test directory is available");
		const auto runner = make_github_cli_process_runner();

		auto request                = base_request(directory, { "--bpr-helper", "stdout", "31" });
		request.StandardOutputLimit = 31U;
		auto result                 = runner->run(request, { });
		expect(
			result.has_value() && result->StandardOutput.size() == 31U,
			"stdout at N bytes succeeds");

		request.Arguments.back() = "32";
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::standard_output_limit,
			"stdout at N+1 bytes fails atomically");

		request                    = base_request(directory, { "--bpr-helper", "stderr", "31" });
		request.StandardErrorLimit = 31U;
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::unexpected_standard_error,
			"stderr at N bytes is a fixed unexpected-output failure");

		request.Arguments.back() = "32";
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::standard_error_limit,
			"stderr at N+1 bytes is a fixed limit failure");

		request                     = base_request(directory, { "--bpr-helper", "both", "131072" });
		request.StandardOutputLimit = 1024U;
		request.StandardErrorLimit  = 1024U;
		result                      = runner->run(request, { });
		expect(
			!result.has_value()
				&& (result.error()
						== github_cli_process_failure::standard_output_limit
					|| result.error()
						== github_cli_process_failure::standard_error_limit),
			"simultaneous stdout and stderr flood is drained and bounded");

		request = base_request(directory, { "--bpr-helper", "nonzero" });
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::nonzero_exit,
			"nonzero exit exposes no partial stdout");

		request = base_request(directory, { "--bpr-helper", "crash" });
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::process_crash,
			"crash exposes no partial stdout");
	}

#if defined(Q_OS_UNIX)
	[[nodiscard]] auto read_descendant_pid(const QString& aPath) -> pid_t
	{
		QFile file{ aPath };
		expect(file.open(QIODevice::ReadOnly), "descendant pid file is readable");
		bool       valid = false;
		const auto value = file.readAll().trimmed().toLongLong(&valid);
		expect(
			valid && value > 1
				&& value
					<= static_cast<qlonglong>(
						std::numeric_limits<pid_t>::max()),
			"descendant pid is bounded and valid");
		return static_cast<pid_t>(value);
	}

	[[nodiscard]] auto wait_until_process_is_gone(const pid_t aProcess) -> bool
	{
		const auto deadline = std::chrono::steady_clock::now() + 2s;
		while (std::chrono::steady_clock::now() < deadline) {
			errno = 0;
			if (::kill(aProcess, 0) == -1 && errno == ESRCH) {
				return true;
			}
			std::this_thread::sleep_for(10ms);
		}
		return false;
	}

	auto test_unix_process_hardening() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "hardening test directory is available");
		const auto runner = make_github_cli_process_runner();

		const auto inheritedDescriptor = ::open("/dev/null", O_RDONLY);
		expect(
			inheritedDescriptor >= 3,
			"non-standard inherited descriptor is available");
		const auto descriptorFlags = ::fcntl(inheritedDescriptor, F_GETFD);
		expect(
			descriptorFlags >= 0
				&& ::fcntl(
					   inheritedDescriptor,
					   F_SETFD,
					   descriptorFlags & ~FD_CLOEXEC)
					== 0,
			"inherited descriptor close-on-exec is explicitly disabled");

		struct sigaction priorDisposition{ };
		struct sigaction ignoredDisposition{ };
		ignoredDisposition.sa_handler = SIG_IGN;
		expect(
			sigemptyset(&ignoredDisposition.sa_mask) == 0
				&& ::sigaction(
					   SIGUSR1,
					   &ignoredDisposition,
					   &priorDisposition)
					== 0,
			"parent signal disposition is changed for reset verification");

		struct rlimit priorCoreLimit{ };
		expect(
			::getrlimit(RLIMIT_CORE, &priorCoreLimit) == 0,
			"parent core limit is available");
		const auto canaryCoreLimit = priorCoreLimit.rlim_max == RLIM_INFINITY
			? static_cast<rlim_t>(1)
			: std::min<rlim_t>(
				  priorCoreLimit.rlim_max,
				  static_cast<rlim_t>(1));
		expect(
			canaryCoreLimit > 0,
			"parent permits a nonzero core-limit canary");
		auto raisedCoreLimit     = priorCoreLimit;
		raisedCoreLimit.rlim_cur = canaryCoreLimit;
		expect(
			::setrlimit(RLIMIT_CORE, &raisedCoreLimit) == 0,
			"parent core limit is raised for disable verification");

		auto request = base_request(
			directory,
			{
				"--bpr-helper",
				"unix-hardening",
				std::to_string(inheritedDescriptor),
			});
		request.StandardOutputLimit = 8U;
		const auto result           = runner->run(request, { });

		const bool coreLimitRestored = ::setrlimit(RLIMIT_CORE, &priorCoreLimit) == 0;
		const bool signalRestored    = ::sigaction(SIGUSR1, &priorDisposition, nullptr) == 0;
		const bool descriptorClosed  = ::close(inheritedDescriptor) == 0;
		expect(coreLimitRestored, "parent core limit is restored");
		expect(signalRestored, "parent signal disposition is restored");
		expect(descriptorClosed, "parent test descriptor is closed");
		expect(
			result.has_value()
				&& result->StandardOutput == "hardened",
			"Unix child closes inherited descriptors, disables core dumps, "
			"resets signals, and becomes the new session/group leader");
	}

	auto test_descendant_process_group_is_killed() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "descendant test directory is available");
		const auto runner = make_github_cli_process_runner();

		const auto successPidFile = directory.filePath(QStringLiteral("success-descendant.pid"));
		auto       request        = base_request(
			directory,
			{
				"--bpr-helper",
				"success-descendant",
				utf8(successPidFile),
			});
		request.StandardOutputLimit = 5U;
		const auto success          = runner->run(request, { });
		expect(
			success.has_value() && success->StandardOutput == "clean",
			"clean leader exit retains exact successful output");
		const auto successDescendant     = read_descendant_pid(successPidFile);
		const bool successDescendantGone = wait_until_process_is_gone(successDescendant);
		if (!successDescendantGone) {
			static_cast<void>(::kill(successDescendant, SIGKILL));
		}
		expect(
			successDescendantGone,
			"clean leader exit kills its remaining process-group descendant");

		const auto failurePidFile = directory.filePath(QStringLiteral("failure-descendant.pid"));
		request                   = base_request(
			directory,
			{
				"--bpr-helper",
				"failure-descendant",
				utf8(failurePidFile),
			});
		request.StandardOutputLimit = 10U;
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::nonzero_exit,
			"nonzero leader exit exposes no output and returns a fixed failure");
		const auto failureDescendant     = read_descendant_pid(failurePidFile);
		const bool failureDescendantGone = wait_until_process_is_gone(failureDescendant);
		if (!failureDescendantGone) {
			static_cast<void>(::kill(failureDescendant, SIGKILL));
		}
		expect(
			failureDescendantGone,
			"nonzero leader exit kills its remaining process-group descendant");

		const auto timeoutPidFile = directory.filePath(QStringLiteral("timeout-descendant.pid"));
		request                   = base_request(
			directory,
			{
				"--bpr-helper",
				"descendant",
				utf8(timeoutPidFile),
			});
		request.Timeout = 500ms;
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::timeout,
			"timeout kills and reaps the process-group leader");
		const auto timeoutDescendant     = read_descendant_pid(timeoutPidFile);
		const bool timeoutDescendantGone = wait_until_process_is_gone(timeoutDescendant);
		if (!timeoutDescendantGone) {
			static_cast<void>(::kill(timeoutDescendant, SIGKILL));
		}
		expect(
			timeoutDescendantGone,
			"timeout kills descendants remaining in the initial process group");

		const auto cancelledPidFile = directory.filePath(QStringLiteral("cancelled-descendant.pid"));
		request                     = base_request(
			directory,
			{
				"--bpr-helper",
				"descendant",
				utf8(cancelledPidFile),
			});
		std::stop_source cancellation;
		std::thread      canceller{
			[&] {
				const auto deadline = std::chrono::steady_clock::now() + 1s;
				while (
					std::chrono::steady_clock::now() < deadline
					&& QFileInfo{ cancelledPidFile }.size() <= 0) {
					std::this_thread::sleep_for(10ms);
				}
				cancellation.request_stop();
			}
		};
		const auto cancelled = runner->run(request, cancellation.get_token());
		canceller.join();
		expect(
			!cancelled.has_value()
				&& cancelled.error()
					== github_cli_process_failure::cancelled,
			"cancellation kills and reaps the process-group leader");
		const auto cancelledDescendant     = read_descendant_pid(cancelledPidFile);
		const bool cancelledDescendantGone = wait_until_process_is_gone(cancelledDescendant);
		if (!cancelledDescendantGone) {
			static_cast<void>(::kill(cancelledDescendant, SIGKILL));
		}
		expect(
			cancelledDescendantGone,
			"cancellation kills descendants remaining in the initial "
			"process group");

		const auto escapedPidFile = directory.filePath(QStringLiteral("escaped-group-descendant.pid"));
		request                   = base_request(
			directory,
			{
				"--bpr-helper",
				"escaped-group-descendant",
				utf8(escapedPidFile),
			});
		request.StandardOutputLimit = 5U;
		const auto escapedResult    = runner->run(request, { });
		expect(
			escapedResult.has_value()
				&& escapedResult->StandardOutput == "clean",
			"leader succeeds after its descendant moves to another group");
		const auto escapedDescendant         = read_descendant_pid(escapedPidFile);
		errno                                = 0;
		const bool escapedDescendantSurvived = ::kill(escapedDescendant, 0) == 0
			|| errno == EPERM;
		static_cast<void>(::kill(escapedDescendant, SIGKILL));
		const bool escapedDescendantCleaned = wait_until_process_is_gone(escapedDescendant);
		expect(
			escapedDescendantSurvived,
			"cleanup guarantee intentionally excludes descendants that leave "
			"the initial process group");
		expect(
			escapedDescendantCleaned,
			"escaped-group fixture performs explicit same-test cleanup");
	}

	auto test_expired_queued_request_never_launches() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "queued test directory is available");
		const auto runner = make_github_cli_process_runner();

		const auto blockerPidFile = directory.filePath(QStringLiteral("blocker-descendant.pid"));
		auto       blockerRequest = base_request(
			directory,
			{
				"--bpr-helper",
				"descendant",
				utf8(blockerPidFile),
			});
		blockerRequest.Timeout = 2s;

		std::stop_source                         blockerCancellation;
		std::optional<github_cli_process_result> blockerResult;
		std::thread                              blocker{
			[&] {
				blockerResult.emplace(runner->run(
					blockerRequest,
					blockerCancellation.get_token()));
			}
		};

		const auto readyDeadline = std::chrono::steady_clock::now() + 1s;
		while (
			std::chrono::steady_clock::now() < readyDeadline
			&& QFileInfo{ blockerPidFile }.size() <= 0) {
			std::this_thread::sleep_for(10ms);
		}
		const bool blockerStarted = QFileInfo{ blockerPidFile }.size() > 0;
		if (!blockerStarted) {
			blockerCancellation.request_stop();
			blocker.join();
		}
		expect(blockerStarted, "blocking process starts before queue test");

		const auto marker         = directory.filePath(QStringLiteral("expired-request.marker"));
		auto       expiredRequest = base_request(
			directory,
			{
				"--bpr-helper",
				"touch",
				utf8(marker),
			});
		expiredRequest.Timeout = 50ms;
		expect_failure(
			*runner,
			expiredRequest,
			github_cli_process_failure::timeout,
			"queued request expires without entering the process worker");

		blockerCancellation.request_stop();
		blocker.join();
		expect(
			blockerResult.has_value()
				&& !blockerResult->has_value()
				&& blockerResult->error()
					== github_cli_process_failure::cancelled,
			"blocking process is cancelled after queue expiry");
		std::this_thread::sleep_for(50ms);
		expect(
			!QFileInfo{ marker }.exists(),
			"expired queued request never launches later");

		const auto descendant     = read_descendant_pid(blockerPidFile);
		const bool descendantGone = wait_until_process_is_gone(descendant);
		if (!descendantGone) {
			static_cast<void>(::kill(descendant, SIGKILL));
		}
		expect(
			descendantGone,
			"queue blocker descendant is contained during cancellation");
	}
#endif

	class blocking_process_runner_test_seam final
		: public github_cli_process_runner_test_seam
	{
	public:
		auto before_execute() -> void override
		{
			std::unique_lock lock{ Mutex };
			Entered = true;
			StateChanged.notify_all();
			StateChanged.wait(
				lock,
				[&] {
					return Released;
				});
		}

		[[nodiscard]] auto wait_until_entered() -> bool
		{
			std::unique_lock lock{ Mutex };
			return StateChanged.wait_for(
				lock,
				1s,
				[&] {
					return Entered;
				});
		}

		auto release() -> void
		{
			{
				std::lock_guard lock{ Mutex };
				Released = true;
			}
			StateChanged.notify_all();
		}

	private:
		std::mutex              Mutex;
		std::condition_variable StateChanged;
		bool                    Entered  = false;
		bool                    Released = false;
	};

	auto test_watchdog_failure_quarantines_worker() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "watchdog test directory is available");
		const auto testSeam = std::make_shared<blocking_process_runner_test_seam>();
		const auto runner   = make_github_cli_process_runner_for_test(testSeam);

		const auto blockedMarker  = directory.filePath(QStringLiteral("blocked.marker"));
		auto       blockedRequest = base_request(
			directory,
			{
				"--bpr-helper",
				"touch",
				utf8(blockedMarker),
			});
		blockedRequest.Timeout     = 20ms;
		blockedRequest.ReapTimeout = 1ms;

		std::optional<github_cli_process_result> blockedResult;
		std::thread                              blockedCaller{
			[&] {
				blockedResult.emplace(
					runner->run(blockedRequest, { }));
			}
		};
		const bool workerStalled = testSeam->wait_until_entered();
		expect(
			workerStalled,
			"test seam stalls the sole worker before execution");
		blockedCaller.join();
		expect(
			blockedResult.has_value()
				&& !blockedResult->has_value()
				&& blockedResult->error()
					== github_cli_process_failure::reap_failure,
			"completion watchdog returns a fixed reap failure");
		expect(
			!QFileInfo{ blockedMarker }.exists(),
			"watchdog-expired request has not launched");

		const auto quarantinedMarker  = directory.filePath(QStringLiteral("quarantined.marker"));
		auto       quarantinedRequest = base_request(
			directory,
			{
				"--bpr-helper",
				"touch",
				utf8(quarantinedMarker),
			});
		const auto quarantinedResult = runner->run(quarantinedRequest, { });
		expect(
			!quarantinedResult.has_value()
				&& quarantinedResult.error()
					== github_cli_process_failure::reap_failure,
			"a second caller observes the quarantined busy worker");
		expect(
			!QFileInfo{ quarantinedMarker }.exists(),
			"quarantined request never launches");

		testSeam->release();
		const auto recoveredMarker  = directory.filePath(QStringLiteral("recovered.marker"));
		auto       recoveredRequest = base_request(
			directory,
			{
				"--bpr-helper",
				"touch",
				utf8(recoveredMarker),
			});
		github_cli_process_result recovered = std::unexpected{
			github_cli_process_failure::reap_failure
		};
		const auto recoveryDeadline = std::chrono::steady_clock::now() + 2s;
		while (
			std::chrono::steady_clock::now() < recoveryDeadline
			&& !recovered.has_value()) {
			recovered = runner->run(recoveredRequest, { });
			if (
				!recovered.has_value()
				&& recovered.error()
					== github_cli_process_failure::reap_failure) {
				std::this_thread::sleep_for(10ms);
				continue;
			}
			break;
		}
		expect(
			recovered.has_value()
				&& QFileInfo{ recoveredMarker }.exists(),
			"worker reopens only after the stalled execution reaches terminal");
	}

	auto test_deadline_cancellation_and_startup() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "lifecycle test directory is available");
		const auto runner = make_github_cli_process_runner();

		auto request    = base_request(directory, { "--bpr-helper", "hang" });
		request.Timeout = 50ms;
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::timeout,
			"total process deadline kills and reaps the child");

		request.Timeout = 2s;
		std::stop_source cancellation;
		std::thread      canceller{
			[&cancellation] {
				std::this_thread::sleep_for(50ms);
				cancellation.request_stop();
			}
		};
		const auto cancelled = runner->run(request, cancellation.get_token());
		canceller.join();
		expect(
			!cancelled.has_value()
				&& cancelled.error()
					== github_cli_process_failure::cancelled,
			"stop token cancellation kills and reaps the child");

#if defined(_WIN32)
		const auto invalidChildName = QStringLiteral("invalid-child.exe");
#else
		const auto invalidChildName = QStringLiteral("invalid-child");
#endif
		const auto invalidExecutable = directory.filePath(invalidChildName);
		QFile      file{ invalidExecutable };
		expect(file.open(QIODevice::WriteOnly), "invalid executable is created");
		expect(file.write("not-an-executable", 17) == 17, "invalid image is written");
		file.close();
		expect(
			QFile::setPermissions(
				invalidExecutable,
				QFileDevice::ReadOwner | QFileDevice::WriteOwner
					| QFileDevice::ExeOwner),
			"invalid image is executable");

		request            = base_request(directory, { "ignored" });
		request.Executable = utf8(QFileInfo{ invalidExecutable }.canonicalFilePath());
		expect_failure(
			*runner,
			request,
			github_cli_process_failure::start_failure,
			"executable image startup failure is fixed and redacted");

		request = base_request(directory, { "--bpr-helper", "hang" });
		std::stop_source alreadyCancelled;
		alreadyCancelled.request_stop();
		const auto preCancelled = runner->run(request, alreadyCancelled.get_token());
		expect(
			!preCancelled.has_value()
				&& preCancelled.error()
					== github_cli_process_failure::cancelled,
			"pre-start cancellation launches no child");
	}
}

auto main(int argc, char* argv[]) -> int
{
#if defined(Q_OS_UNIX)
	if (
		argc >= 2
		&& std::string_view{ argv[1] } == "--bpr-helper") {
		QStringList arguments;
		arguments.reserve(static_cast<qsizetype>(argc));
		for (int index = 0; index < argc; ++index) {
			arguments.push_back(QString::fromUtf8(argv[index]));
		}
		return helper_main(arguments);
	}
#endif

	QCoreApplication application{ argc, argv };
	const auto       arguments = QCoreApplication::arguments();
	if (
		arguments.size() >= 2
		&& arguments.at(1) == QStringLiteral("--bpr-helper")) {
		return helper_main(arguments);
	}

	test_exact_process_boundary();
	test_bare_repository_working_directory_is_rejected();
	test_shape_limits_reject_without_launch();
	test_stream_limits_and_redaction();
#if defined(Q_OS_UNIX)
	test_unix_process_hardening();
	test_descendant_process_group_is_killed();
	test_expired_queued_request_never_launches();
#endif
	test_watchdog_failure_quarantines_worker();
	test_deadline_cancellation_and_startup();
	std::cout << "GitHub CLI process runner tests passed\n";
	return EXIT_SUCCESS;
}
