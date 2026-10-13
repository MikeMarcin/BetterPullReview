#include "../support/test_expectation.hpp"
#include "github_cli_process_runner.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
#if defined(Q_OS_MACOS)
	#include <sys/syslimits.h>
#endif

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
	#include <fcntl.h>
	#include <io.h>
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

	using bpr::tests::expect;

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
		if (mode == QStringLiteral("sensitive")) {
			const auto size = aArguments.size() > 3 ? parse_count(aArguments.at(3)) : 32U;
			std::cout << std::string(size, 'q') << '\n';
			return 0;
		}
		if (mode == QStringLiteral("credential-check") || mode == QStringLiteral("credential-echo") || mode == QStringLiteral("credential-prefix") || mode == QStringLiteral("credential-split")) {
			const auto secret = qEnvironmentVariable("GH_TOKEN");
			if (secret != QString(32, QChar{ 'q' }) || !qEnvironmentVariable("GITHUB_TOKEN").isEmpty()) {
				return 94;
			}
			if (mode == QStringLiteral("credential-echo")) {
				std::cout << secret.toStdString();
			} else if (mode == QStringLiteral("credential-prefix")) {
				std::cout << std::string(31U, 'q') << 'x';
			} else if (mode == QStringLiteral("credential-split")) {
				std::cout << std::string(16U, 'q') << std::flush;
				std::this_thread::sleep_for(20ms);
				std::cout << std::string(16U, 'q');
			} else {
				std::cout << "verified";
			}
			return 0;
		}

		if (mode == QStringLiteral("exact")) {
#if defined(_WIN32)
			constexpr auto exactArgumentCount = 8;
#else
			constexpr auto exactArgumentCount = 7;
#endif
			if (aArguments.size() != exactArgumentCount) {
				return 92;
			}
			// The parent sees only "ok" or not, so the member that differed is
			// written beside the working directory the parent chose, where the
			// parent prints it on failure (BPR-0438).
			const auto record = [&aArguments](const QString& aReason) {
				QFile file{ QDir{ aArguments.at(6) }.filePath(
					QStringLiteral("exact-boundary.txt")) };
				if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
					return;
				}
				auto text = aReason + QStringLiteral("\ncwd=")
					+ QDir::current().canonicalPath() + QStringLiteral("\n");
				const auto environment = QProcessEnvironment::systemEnvironment();
				for (const auto& name : environment.keys()) {
					text += name + QStringLiteral("=") + environment.value(name)
						+ QStringLiteral("\n");
				}
				file.write(text.toUtf8());
			};
			if (!exact_environment_matches(aArguments.at(5), aArguments)) {
				record(QStringLiteral("environment differs"));
				return 93;
			}
			if (QDir::current().canonicalPath() != aArguments.at(6)) {
				record(QStringLiteral("working directory differs"));
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
				record(
					QStringLiteral("argv or stdin differs: argv=")
					+ aArguments.join(QStringLiteral("|"))
					+ QStringLiteral(" stdin_bytes=")
					+ QString::number(input.size()));
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

		if (mode == QStringLiteral("http-error")) {
			std::cout << "HTTP/2.0 401 Unauthorized\r\n\r\n{}";
			std::cerr << "synthetic-private-diagnostic";
			return 1;
		}
#if defined(Q_OS_UNIX)
		if (mode == QStringLiteral("pid-hang") && aArguments.size() == 4) {
			QFile pidFile{ aArguments.at(3) };
			if (!pidFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
				|| pidFile.write(QByteArray::number(static_cast<qlonglong>(::getpid()))) <= 0) {
				return 114;
			}
			pidFile.close();
			std::this_thread::sleep_for(30s);
			return 0;
		}
#endif
		if (mode == QStringLiteral("hang")) {
			std::cout << "partial" << std::flush;
			std::this_thread::sleep_for(10s);
			return 0;
		}
		if (mode == QStringLiteral("churn-store")) {
			const auto until = std::chrono::steady_clock::now() + 500ms;
			do {
				for (auto index = 0; index < 32; ++index) {
					QFile file{ QStringLiteral("pack-%1.lock").arg(index) };
					if (!file.open(QIODevice::WriteOnly) || file.write("x", 1) != 1) {
						return 112;
					}
				}
				for (auto index = 0; index < 32; ++index) {
					if (!QFile::remove(QStringLiteral("pack-%1.lock").arg(index))) {
						return 113;
					}
				}
			} while (std::chrono::steady_clock::now() < until);
			std::cout << "churned";
			return 0;
		}
		if (mode == QStringLiteral("nonzero")) {
			std::cout << "discard-me";
			return 7;
		}
		if (mode == QStringLiteral("hold")) {
			// [[BPR-1114]]. Says it is running, then stays alive so the parent
			// can look at what it holds.
			if (aArguments.size() < 4) {
				return 114;
			}
			QFile marker{ aArguments.at(3) };
			if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate) || marker.write("started", 7) != 7) {
				return 115;
			}
			marker.close();
			std::this_thread::sleep_for(10s);
			return 0;
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
			// Qt reports a Windows child as crashed only when its exit code
			// is an NTSTATUS failure; 137 is an ordinary exit here, and the
			// runner rightly reported it as one. STATUS_ACCESS_VIOLATION is
			// what an actual fault would leave (BPR-0438).
			static_cast<void>(
				TerminateProcess(GetCurrentProcess(), 0xC0000005U));
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

		// [[BPR-1252]]. Reports the soft descriptor limit this child was
		// started with, after checking that the named descriptor, open and
		// inheritable in the parent, is not open here.
		if (mode == QStringLiteral("descriptor-bound")) {
			if (aArguments.size() != 4) {
				return 105;
			}
			bool       validDescriptor     = false;
			const auto inheritedDescriptor = aArguments.at(3).toInt(&validDescriptor);
			errno                          = 0;
			if (!validDescriptor || ::fcntl(inheritedDescriptor, F_GETFD) != -1 || errno != EBADF) {
				return 106;
			}
			struct rlimit descriptorLimit{ };
			if (::getrlimit(RLIMIT_NOFILE, &descriptorLimit) != 0) {
				return 107;
			}
			std::cout << descriptorLimit.rlim_cur;
			return 0;
		}

		if (
			mode == QStringLiteral("descendant")
			|| mode == QStringLiteral("credential-descendant")
			|| mode == QStringLiteral("success-descendant")
			|| mode == QStringLiteral("failure-descendant")
			|| mode == QStringLiteral("escaped-group-descendant")) {
			if (aArguments.size() != 4) {
				return 98;
			}
			const bool cleanSuccess = mode == QStringLiteral("success-descendant");
			const bool fixedFailure = mode == QStringLiteral("failure-descendant");
			const bool escapedGroup = mode == QStringLiteral("escaped-group-descendant");
			const bool credentialed = mode == QStringLiteral("credential-descendant");
			if (credentialed && qEnvironmentVariable("GH_TOKEN") != QString(32, QChar{ 'q' })) {
				return 94;
			}
			const auto descendant = ::fork();
			if (descendant < 0) {
				return 99;
			}
			if (descendant == 0) {
				if (credentialed) {
					QFile verified{ aArguments.at(3) + QStringLiteral(".verified") };
					if (qEnvironmentVariable("GH_TOKEN") != QString(32, QChar{ 'q' })
						|| !verified.open(QIODevice::WriteOnly) || verified.write("verified") != 8) {
						return 94;
					}
					verified.close();
				}
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
		if (!result.has_value() || result->StandardOutput != "ok") {
			// The child records which member differed and what it saw; name
			// it before failing rather than reporting the four as a set.
			QFile recorded{ directory.filePath(QStringLiteral("exact-boundary.txt")) };
			if (recorded.open(QIODevice::ReadOnly)) {
				std::cerr << "exact process boundary differed:\n"
						  << recorded.readAll().toStdString() << '\n';
			} else if (!result.has_value()) {
				std::cerr << "exact process boundary run failed with code "
						  << static_cast<unsigned int>(result.error()) << '\n';
			}
		}

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

	// [[BPR-0807]]. The Git-acquisition factory is the only way a runner gets
	// file limits, so it refuses a store it cannot bound before any runner
	// exists.
	auto test_git_acquisition_factory_refuses_unbounded_stores() -> void
	{
		QTemporaryDir store;
		QTemporaryDir elsewhere;
		expect(store.isValid() && elsewhere.isValid(), "acquisition stores are created");
		const auto directory = std::filesystem::path{ QDir{ store.path() }.absolutePath().toStdWString() };
		const auto other     = std::filesystem::path{ QDir{ elsewhere.path() }.absolutePath().toStdWString() };
		const auto valid     = github_cli_process_file_limits{
			.MaximumFileBytes        = 1024U * 1024U,
			.Directory               = directory,
			.MaximumDirectoryBytes   = 96U * 1024U * 1024U,
			.MaximumDirectoryEntries = 128U,
		};
		const auto refused = [&valid](const auto& aChange) {
			auto limits = valid;
			aChange(limits);
			return make_github_cli_git_acquisition_process_runner(std::move(limits)) == nullptr;
		};
		expect(make_github_cli_git_acquisition_process_runner(valid) != nullptr,
			   "a bounded absolute store makes a runner");
		expect(!refused([&directory](github_cli_process_file_limits& aLimits) { aLimits.StandardOutputFile = directory / "wire.pack"; }),
			   "a fresh spool inside the store makes a runner");
		expect(refused([](github_cli_process_file_limits& aLimits) { aLimits.MaximumFileBytes = 0U; }),
			   "a zero per-file limit is refused");
		expect(refused([&directory](github_cli_process_file_limits& aLimits) { aLimits.StandardOutputFile = directory / ".." / "x" / "wire.pack"; }),
			   "a spool that climbs out of the store is refused");
		expect(refused([&other](github_cli_process_file_limits& aLimits) { aLimits.StandardOutputFile = other / "wire.pack"; }),
			   "a spool in another absolute directory is refused");
		QFile existing{ QDir{ store.path() }.filePath(QStringLiteral("existing.pack")) };
		expect(existing.open(QIODevice::WriteOnly), "a spool file exists before the runner");
		existing.close();
		expect(refused([&directory](github_cli_process_file_limits& aLimits) { aLimits.StandardOutputFile = directory / "existing.pack"; }),
			   "a spool over an existing file is refused");
		expect(refused([](github_cli_process_file_limits& aLimits) { aLimits.Directory = "relative-store"; }),
			   "a relative store is refused");
	}

	auto test_git_acquisition_store_sampling() -> void
	{
		QTemporaryDir store;
		expect(store.isValid(), "sampled acquisition store is created");
		const auto directory = std::filesystem::path{ QDir{ store.path() }.absolutePath().toStdWString() };
		const auto limits    = github_cli_process_file_limits{
			.MaximumFileBytes        = 1024U,
			.Directory               = directory,
			.MaximumDirectoryBytes   = 4096U,
			.MaximumDirectoryEntries = 128U,
		};
		const auto runner = make_github_cli_git_acquisition_process_runner(limits);
		expect(runner != nullptr, "bounded acquisition sampler is available");
		const auto churned = runner->run(base_request(store, { "--bpr-helper", "churn-store" }), { });
		expect(churned && churned->StandardOutput == "churned",
			   "temporary files removed during sampling are not byte-limit violations");
		QFile pack{ QDir{ store.path() }.filePath(QStringLiteral("oversized.pack")) };
		expect(pack.open(QIODevice::WriteOnly) && pack.resize(1024), "per-file boundary fixture is created");
		pack.close();
		expect_failure(*runner, base_request(store, { "--bpr-helper", "hang" }),
					   github_cli_process_failure::standard_output_limit,
					   "a retained file at the acquisition limit is still refused");
		expect(pack.remove(), "per-file fixture is removed");
		for (auto index = 0; index < 5; ++index) {
			QFile entry{ QDir{ store.path() }.filePath(QStringLiteral("aggregate-%1").arg(index)) };
			expect(entry.open(QIODevice::WriteOnly) && entry.resize(900), "aggregate-cap fixture is created");
		}
		expect_failure(*runner, base_request(store, { "--bpr-helper", "hang" }),
					   github_cli_process_failure::standard_output_limit,
					   "the aggregate byte limit remains enforced");
		for (auto index = 0; index < 5; ++index) {
			expect(QFile::remove(QDir{ store.path() }.filePath(QStringLiteral("aggregate-%1").arg(index))),
				   "aggregate-cap fixture is removed");
		}
#if defined(Q_OS_UNIX)
		std::error_code linkError;
		const auto      link = directory / "outside-link";
		std::filesystem::create_symlink(directory.parent_path(), link, linkError);
		expect(!linkError, "symlink refusal fixture is created");
		expect_failure(*runner, base_request(store, { "--bpr-helper", "hang" }),
					   github_cli_process_failure::standard_output_limit,
					   "symlinks still refuse instead of being followed by the sampler");
		expect(std::filesystem::remove(link, linkError) && !linkError, "symlink refusal fixture is removed");
#endif
		for (auto index = 0; index < 129; ++index) {
			QFile entry{ QDir{ store.path() }.filePath(QStringLiteral("entry-%1").arg(index)) };
			expect(entry.open(QIODevice::WriteOnly), "entry-cap fixture is created");
		}
		expect_failure(*runner, base_request(store, { "--bpr-helper", "hang" }),
					   github_cli_process_failure::standard_output_limit,
					   "the directory entry limit remains enforced after transient-file recovery");
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

	/// One owned credential never crosses the generic request/result seam.
	auto test_sensitive_execution() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "sensitive course owns its temporary directory");
		auto runner                 = make_github_cli_process_runner();
		auto request                = base_request(directory, { "--bpr-helper", "sensitive" });
		request.StandardOutputLimit = cli_sensitive_bytes::capacity;
		auto credential             = runner->acquire_sensitive(request, { });
		expect(credential && (*credential)->view().size() == 32U, "sensitive stdout is acquired without generic result output");
		const std::weak_ptr<const cli_sensitive_bytes> lifetime = *credential;
		auto                                           check    = base_request(directory, { "--bpr-helper", "credential-check" });
		const auto                                     result   = runner->run_credentialed(check, *credential, cli_credential_environment::github, { });
		expect(result && result->StandardOutput == "verified" && check.Environment.size() == request.Environment.size(), "only the child receives the credential, while generic request and result remain token-free");
		check.Arguments.back() = "credential-echo";
		const auto echoed      = runner->run_credentialed(check, *credential, cli_credential_environment::github, { });
		expect(!echoed, "a child echoing its credential cannot expose generic output");
		check.Arguments.back() = "credential-split";
		expect(!runner->run_credentialed(check, *credential, cli_credential_environment::github, { }), "a credential split across channel reads cannot reach generic output");
		check.Arguments.back() = "credential-prefix";
		const auto prefix      = runner->run_credentialed(check, *credential, cli_credential_environment::github, { });
		expect(prefix && prefix->StandardOutput == std::string(31U, 'q') + 'x', "overlapping partial matches preserve ordinary output without copying a complete credential");
		check.Environment.push_back({ "GITHUB_TOKEN", "other" });
		expect(!runner->run_credentialed(check, *credential, cli_credential_environment::github, { }), "conflicting generic credential variables refuse before launch");
		for (const auto count : { 0U, 8192U, 8193U }) {
			request.Arguments = { "--bpr-helper", "sensitive", std::to_string(count) };
			auto answer       = runner->acquire_sensitive(request, { });
			expect(answer.has_value() == (count == 8192U), "empty and oversized sensitive results refuse; exact eight-KiB value is admitted");
		}
		std::stop_source stop;
		stop.request_stop();
		expect(!runner->acquire_sensitive(request, stop.get_token()), "pre-cancelled acquisition launches no child");
		credential = std::unexpected{ github_cli_process_failure::cancelled };
		runner.reset();
		expect(lifetime.expired(), "runner drain releases the operation credential");
		alignas(cli_sensitive_bytes) std::array<std::byte, sizeof(cli_sensitive_bytes)> storage{ };
		auto*                                                                           bytes = std::construct_at(reinterpret_cast<cli_sensitive_bytes*>(storage.data()));
		expect(bytes->append("runtime-canary"), "canary enters owned sensitive storage");
		std::destroy_at(bytes);
		expect(std::ranges::all_of(storage, [](std::byte aByte) { return aByte == std::byte{ }; }), "destruction overwrites complete sensitive storage");
	}

	auto test_completed_output_opt_in() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "completed output directory is available");
		const auto runner  = make_github_cli_process_runner();
		auto       request = base_request(directory, { "--bpr-helper", "http-error" });
		expect_failure(*runner, request, github_cli_process_failure::nonzero_exit,
					   "default rejects completed HTTP error output");
		request.PreserveCompletedOutput = true;
		const auto result               = runner->run(request, { });
		expect(result.has_value() && result->ExitCode == 1
				   && result->StandardOutput == "HTTP/2.0 401 Unauthorized\r\n\r\n{}",
			   "opt-in preserves only exact bounded stdout and completed exit code");
		request.StandardOutputLimit = 1U;
		expect_failure(*runner, request, github_cli_process_failure::standard_output_limit,
					   "opt-in stdout overflow exposes no partial output");
		request.StandardOutputLimit = 64U;
		request.StandardErrorLimit  = 1U;
		expect_failure(*runner, request, github_cli_process_failure::standard_error_limit,
					   "opt-in stderr overflow exposes no partial output");
		request                         = base_request(directory, { "--bpr-helper", "crash" });
		request.PreserveCompletedOutput = true;
		expect_failure(*runner, request, github_cli_process_failure::process_crash,
					   "opt-in crash exposes no partial output");
		request                         = base_request(directory, { "--bpr-helper", "hang" });
		request.PreserveCompletedOutput = true;
		request.Timeout                 = 50ms;
		expect_failure(*runner, request, github_cli_process_failure::timeout,
					   "opt-in timeout exposes no partial output");
		request.Timeout = 2s;
		std::stop_source cancellation;
		std::thread      canceller{ [&] {
			std::this_thread::sleep_for(50ms);
			cancellation.request_stop();
		} };
		const auto cancelled = runner->run(request, cancellation.get_token());
		canceller.join();
		expect(!cancelled.has_value() && cancelled.error() == github_cli_process_failure::cancelled,
			   "opt-in cancellation exposes no partial output");
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
					   static_cast<int>(static_cast<unsigned>(descriptorFlags) & ~static_cast<unsigned>(FD_CLOEXEC)))
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

	#if defined(Q_OS_MACOS)
	/// \brief [[BPR-1252]]. A launch bounds its child's descriptor sweep on
	/// macOS without letting an open descriptor through.
	///
	/// \details Qt's child closes each number up to its soft descriptor limit.
	/// The runner's child lowers that limit, before the sweep, to one past its
	/// highest open descriptor and never below `OPEN_MAX`. So an inheritable
	/// descriptor above `OPEN_MAX` must still be closed, and must be what sets
	/// the child's limit; without it the child gets `OPEN_MAX`; and a limit
	/// already below that is left alone. Elsewhere the bound does nothing.
	auto test_unix_descriptor_sweep_is_bounded() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "descriptor bound test directory is available");
		const auto runner = make_github_cli_process_runner();

		struct rlimit priorLimit{ };
		expect(::getrlimit(RLIMIT_NOFILE, &priorLimit) == 0, "parent descriptor limit is available");
		constexpr rlim_t raised = 65536U;
		constexpr int    high   = 20000;
		expect(priorLimit.rlim_max == RLIM_INFINITY || priorLimit.rlim_max >= raised,
			   "parent may raise its descriptor limit above the bound");
		const auto launch = [&](const int aDescriptor, const rlim_t aLimit) -> std::optional<std::string> {
			auto limit     = priorLimit;
			limit.rlim_cur = aLimit;
			expect(::setrlimit(RLIMIT_NOFILE, &limit) == 0, "parent descriptor limit is set for the launch");
			auto request                = base_request(directory, { "--bpr-helper", "descriptor-bound", std::to_string(aDescriptor) });
			request.StandardOutputLimit = 32U;
			const auto result           = runner->run(request, { });
			expect(::setrlimit(RLIMIT_NOFILE, &priorLimit) == 0, "parent descriptor limit is restored");
			if (!result.has_value()) {
				return std::nullopt;
			}
			return result->StandardOutput;
		};
		auto raisedLimit     = priorLimit;
		raisedLimit.rlim_cur = raised;
		expect(::setrlimit(RLIMIT_NOFILE, &raisedLimit) == 0, "parent descriptor limit is raised");
		const auto opened = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
		expect(opened >= 0 && ::dup2(opened, high) == high && ::close(opened) == 0,
			   "an inheritable descriptor is open above OPEN_MAX");
		expect(::fcntl(high, F_GETFD) == 0, "the high descriptor is inheritable");
		const auto withHigh = launch(high, raised);
		expect(::close(high) == 0, "parent high descriptor is closed");
		expect(withHigh == std::to_string(high + 1),
			   "the child closes an inheritable descriptor above OPEN_MAX and its limit is one past it");

		const auto plain = ::open("/dev/null", O_RDONLY);
		expect(plain >= 3, "an ordinary inheritable descriptor is open");
		const auto withoutHigh = launch(plain, raised);
		const auto lowered     = launch(plain, 256U);
		expect(::close(plain) == 0, "parent ordinary descriptor is closed");
		expect(withoutHigh == std::to_string(OPEN_MAX), "a raised limit reaches the child as OPEN_MAX");
		expect(lowered == std::to_string(256U), "a limit below OPEN_MAX reaches the child unchanged");
	}
	#endif

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

		// [[BPR-1121]]. Until the call below is joined, unwinding would destroy a
		// joinable thread, so a failure in this window exits, as it always did.
		auto untilJoined = bpr::tests::exit_on_failure{ };

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
		untilJoined.end();
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
		explicit blocking_process_runner_test_seam(bool aAfterStarted = false)
			: AfterStarted{ aAfterStarted }
		{
		}
		auto before_execute() -> void override
		{
			if (!AfterStarted) {
				block();
			}
		}
		auto after_started() -> void override
		{
			if (AfterStarted) {
				block();
			}
		}
		auto block() -> void
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
		bool                    AfterStarted = false;
		std::mutex              Mutex;
		std::condition_variable StateChanged;
		bool                    Entered  = false;
		bool                    Released = false;
	};

#if defined(Q_OS_UNIX)
	auto test_live_credentialed_quarantine() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "live quarantine course owns its root");
		const auto seam              = std::make_shared<blocking_process_runner_test_seam>(true);
		auto       runner            = make_github_cli_process_runner_for_test(seam);
		auto       acquisitionRunner = make_github_cli_process_runner();
		auto       lookup            = base_request(directory, { "--bpr-helper", "sensitive" });
		lookup.StandardOutputLimit   = cli_sensitive_bytes::capacity;
		auto acquired                = acquisitionRunner->acquire_sensitive(lookup, { });
		expect(acquired.has_value(), "live quarantine uses production sensitive capture");
		const std::weak_ptr<const cli_sensitive_bytes> lifetime = *acquired;
		const auto                                     pidFile  = directory.filePath(QStringLiteral("live-quarantine.pid"));
		auto                                           request  = base_request(directory, { "--bpr-helper", "credential-descendant", utf8(pidFile) });
		request.Timeout                                         = 500ms;
		request.ReapTimeout                                     = 1ms;
		std::optional<github_cli_process_result> result;
		auto                                     untilJoined = bpr::tests::exit_on_failure{ };
		std::thread                              caller{ [&, credential = std::move(*acquired)] {
			result = runner->run_credentialed(request, credential, cli_credential_environment::github, { });
		} };
		expect(seam->wait_until_entered(), "worker stalls after actual credentialed launch");
		const auto deadline = std::chrono::steady_clock::now() + 400ms;
		while (std::chrono::steady_clock::now() < deadline && (!QFileInfo{ pidFile + QStringLiteral(".verified") }.exists() || QFileInfo{ pidFile }.size() == 0)) {
			std::this_thread::sleep_for(10ms);
		}
		expect(QFileInfo{ pidFile + QStringLiteral(".verified") }.exists() && QFileInfo{ pidFile }.size() > 0,
			   "actual descendant verified inherited credential before watchdog failure");
		caller.join();
		untilJoined.end();
		const auto pid = read_descendant_pid(pidFile);
		expect(result && !*result && result->error() == github_cli_process_failure::reap_failure
				   && !lifetime.expired() && ::kill(pid, 0) == 0,
			   "reap failure retains sensitive owner while actual descendant remains alive");
		seam->release();
		runner.reset();
		expect(wait_until_process_is_gone(pid) && lifetime.expired(), "quarantine drains actual descendant before releasing sensitive owner");
	}

	auto test_credentialed_descendant_lifetime() -> void
	{
		for (const bool cancel : { false, true }) {
			QTemporaryDir directory;
			expect(directory.isValid(), "credential descendant course owns its root");
			auto runner                = make_github_cli_process_runner();
			auto lookup                = base_request(directory, { "--bpr-helper", "sensitive" });
			lookup.StandardOutputLimit = cli_sensitive_bytes::capacity;
			auto acquired              = runner->acquire_sensitive(lookup, { });
			expect(acquired.has_value(), "descendant course acquires through production sensitive capture");
			const std::weak_ptr<const cli_sensitive_bytes> lifetime = *acquired;
			const auto                                     pidFile  = directory.filePath(QStringLiteral("credential-descendant.pid"));
			auto                                           request  = base_request(directory, { "--bpr-helper", "credential-descendant", utf8(pidFile) });
			request.Timeout                                         = 1s;
			std::stop_source                         stop;
			std::optional<github_cli_process_result> result;
			auto                                     untilJoined = bpr::tests::exit_on_failure{ };
			std::thread                              caller{ [&, credential = std::move(*acquired)] {
				result = runner->run_credentialed(request, credential, cli_credential_environment::github, stop.get_token());
			} };
			const auto deadline = std::chrono::steady_clock::now() + 800ms;
			while (std::chrono::steady_clock::now() < deadline && (!QFileInfo{ pidFile + QStringLiteral(".verified") }.exists() || QFileInfo{ pidFile }.size() == 0)) {
				std::this_thread::sleep_for(10ms);
			}
			const bool executed = QFileInfo{ pidFile + QStringLiteral(".verified") }.exists() && QFileInfo{ pidFile }.size() > 0;
			expect(executed && !lifetime.expired(), "an actual live descendant verifies the inherited credential while its sensitive owner remains held");
			if (cancel) {
				stop.request_stop();
			}
			caller.join();
			untilJoined.end();
			expect(result && !*result && result->error() == (cancel ? github_cli_process_failure::cancelled : github_cli_process_failure::timeout), "credential-bearing process and descendant terminate with exact cancellation/timeout");
			const auto pid  = read_descendant_pid(pidFile);
			const bool gone = wait_until_process_is_gone(pid);
			if (!gone) {
				static_cast<void>(::kill(pid, SIGKILL));
			}
			runner.reset();
			expect(gone && lifetime.expired(), "production runner drains the actual credential-bearing descendant before releasing its owned sensitive buffer");
		}
	}
#endif

#if defined(Q_OS_UNIX)
	class kill_surviving_test_seam final
		: public github_cli_process_runner_test_seam
	{
	public:
		auto               before_execute() -> void override { }
		[[nodiscard]] auto child_survives_kill() -> bool override
		{
			return true;
		}
	};

	/// [[BPR-1436]]. A child that outlives every kill is abandoned at the
	/// background reap's deadline. Before, the background reap had no
	/// deadline: the worker waited on the child for good, and destroying the
	/// runner joined that worker and never returned.
	auto test_unreapable_child_is_abandoned() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "unreapable-child course owns its root");
		auto       runner   = make_github_cli_process_runner_for_test(std::make_shared<kill_surviving_test_seam>());
		const auto pidFile  = directory.filePath(QStringLiteral("unreapable.pid"));
		auto       request  = base_request(directory, { "--bpr-helper", "pid-hang", utf8(pidFile) });
		request.Timeout     = 300ms;
		request.ReapTimeout = 100ms;
		const auto started  = std::chrono::steady_clock::now();
		const auto first    = runner->run(request, { });
		expect(!first.has_value() && first.error() == github_cli_process_failure::reap_failure,
			   "a child the bounded reap cannot reap fails with reap_failure");
		const auto pid = read_descendant_pid(pidFile);
		expect(::kill(pid, 0) == 0, "the child outlives the runner's kill-and-reap");

		std::this_thread::sleep_until(started + request.Timeout + request.ReapTimeout + github_cli_process_max_reap_timeout + 500ms);
		const auto marker    = directory.filePath(QStringLiteral("after-abandonment.marker"));
		const auto refusedAt = std::chrono::steady_clock::now();
		const auto refused   = runner->run(base_request(directory, { "--bpr-helper", "touch", utf8(marker) }), { });
		expect(!refused.has_value() && refused.error() == github_cli_process_failure::reap_failure
				   && std::chrono::steady_clock::now() - refusedAt < 1s,
			   "after abandonment a later request fails at once with reap_failure");
		expect(!QFileInfo::exists(marker), "a request after abandonment never launches");

		std::atomic_bool destroyed   = false;
		auto             untilJoined = bpr::tests::exit_on_failure{ };
		std::thread      destroyer{ [&] {
			runner.reset();
			destroyed.store(true);
		} };
		const auto destroyDeadline = std::chrono::steady_clock::now() + 2s;
		while (!destroyed.load() && std::chrono::steady_clock::now() < destroyDeadline) {
			std::this_thread::sleep_for(10ms);
		}
		const bool returned = destroyed.load();
		static_cast<void>(::kill(pid, SIGKILL));
		destroyer.join();
		untilJoined.end();
		expect(returned, "destroying the runner returns although its child never exited");
	}
#endif

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

		// [[BPR-1121]]. Until the call below is joined, unwinding would destroy a
		// joinable thread, so a failure in this window exits, as it always did.
		auto untilJoined = bpr::tests::exit_on_failure{ };

		auto secret = std::make_shared<cli_sensitive_bytes>();
		expect(secret->append("quarantined-synthetic-credential\n") && secret->finish_credential(), "quarantine canary is valid");
		const std::weak_ptr<const cli_sensitive_bytes> secretLifetime = secret;
		std::optional<github_cli_process_result>       blockedResult;
		std::thread                                    blockedCaller{
			[&, credential = std::move(secret)] {
				blockedResult.emplace(
					runner->run_credentialed(blockedRequest, credential, cli_credential_environment::github, { }));
			}
		};
		const bool workerStalled = testSeam->wait_until_entered();
		expect(
			workerStalled,
			"test seam stalls the sole worker before execution");
		blockedCaller.join();
		untilJoined.end();
		expect(
			blockedResult.has_value()
				&& !blockedResult->has_value()
				&& blockedResult->error()
					== github_cli_process_failure::reap_failure,
			"completion watchdog returns a fixed reap failure");
		expect(!secretLifetime.expired(), "watchdog refusal holds sensitive ownership while its worker is quarantined");
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
		expect(secretLifetime.expired(), "sensitive ownership is released only after the quarantined worker reaches terminal");
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

#if defined(_WIN32)
	/// \brief Counts every process this test process creates from now on.
	///
	/// \details The Git process contract's accounting job, again: this process
	/// joins a job of its own, which allows no breakaway, so every process it
	/// creates afterwards is born in that job, and the job's accounting counts
	/// it even after it has exited. A process cannot leave a job, so the one
	/// test that needs this runs last.
	class created_processes final
	{
	public:
		created_processes()
			: Job{ ::CreateJobObjectW(nullptr, nullptr) }
		{
			expect(Job != nullptr, "create the process-creation accounting job");
			expect(
				::AssignProcessToJobObject(Job, ::GetCurrentProcess()) != FALSE,
				"this test process joins its accounting job");
		}
		created_processes(const created_processes&)                    = delete;
		auto operator=(const created_processes&) -> created_processes& = delete;
		~created_processes()
		{
			static_cast<void>(::CloseHandle(Job));
		}

		[[nodiscard]] auto total() const -> DWORD
		{
			JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{ };
			expect(
				::QueryInformationJobObject(Job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr) != FALSE,
				"read the process-creation accounting job");
			return accounting.TotalProcesses;
		}

	private:
		HANDLE Job = nullptr;
	};

	class standard_handle_list_seam final
		: public github_cli_process_runner_test_seam
	{
	public:
		auto before_execute() -> void override { }

		[[nodiscard]] auto fail_standard_handle_list() -> bool override
		{
			return Fail.load();
		}

		std::atomic_bool Fail{ true };
	};

	// [[BPR-1114]]. A child started through the runner inherits its three
	// standard handles and nothing else. The parent holds a C runtime stream
	// on a file, which the C runtime makes inheritable, while the runner
	// starts the child; the stream then closes, and while the child still runs
	// the parent opens the file with no sharing at all. A child that inherited
	// the stream's handle would still hold the file, and the open would be
	// refused with ERROR_SHARING_VIOLATION, as [[BPR-1104]] found for a Git
	// child. The production factory is used: the restriction is not a seam.
	auto test_windows_launch_inherits_only_standard_handles() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "inherited-handle directory is available");
		const auto runner = make_github_cli_process_runner();
		const auto held   = std::filesystem::path{
			QDir{ directory.path() }.filePath(QStringLiteral("held-by-the-parent.bin")).toStdWString()
		};
		{
			std::ofstream output{ held, std::ios::binary };
			output << "held by the parent while the child starts";
		}
		auto stream = std::make_optional<std::ifstream>(held, std::ios::binary);
		expect(stream->is_open(), "the parent holds a C runtime stream on the file");

		const auto marker  = directory.filePath(QStringLiteral("hold-started.marker"));
		auto       request = base_request(directory, { "--bpr-helper", "hold", utf8(marker) });
		request.Timeout    = 8s;
		std::stop_source                         cancellation;
		std::atomic_bool                         returned{ false };
		std::optional<github_cli_process_result> result;
		std::thread                              call{ [&] {
			result.emplace(runner->run(request, cancellation.get_token()));
			returned.store(true);
		} };
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while (!QFileInfo::exists(marker) && !returned.load() && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(10ms);
		}
		const auto running = QFileInfo::exists(marker) && !returned.load();
		stream.reset();

		const auto exclusive = ::CreateFileW(held.c_str(), GENERIC_READ, 0U, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		const auto error     = exclusive == INVALID_HANDLE_VALUE ? ::GetLastError() : DWORD{ ERROR_SUCCESS };
		const auto stillRan  = !returned.load();
		if (exclusive != INVALID_HANDLE_VALUE) {
			static_cast<void>(::CloseHandle(exclusive));
		}
		cancellation.request_stop();
		call.join();
		expect(running, "the child started through the runner is running");
		expect(stillRan, "the exclusive open is made while that child still runs");
		expect(
			error == ERROR_SUCCESS,
			"the parent opens the file with no sharing while the child runs: the child inherited none of "
			"the parent's handles (a refusal is error "
				+ std::to_string(error) + ", and 32 is ERROR_SHARING_VIOLATION)");
		expect(
			result.has_value() && !result->has_value()
				&& result->error() == github_cli_process_failure::cancelled,
			"the held child is cancelled");
		std::cout << "BPR-1114 inherited handle: the parent opened its file with no sharing while the runner's child ran\n";
	}

	// [[BPR-1114]]. The Git-acquisition runner's standard output is a spool
	// file Qt opens, not a pipe. The restriction's list names that file handle
	// like any other standard handle, so the child still starts and writes its
	// output into the spool.
	auto test_windows_spooled_launch_keeps_its_file() -> void
	{
		QTemporaryDir store;
		expect(store.isValid(), "spool store is available");
		const auto directory = std::filesystem::path{ QDir{ store.path() }.absolutePath().toStdWString() };
		const auto spool     = directory / "wire.pack";
		const auto runner    = make_github_cli_git_acquisition_process_runner({
			.MaximumFileBytes        = 1024U,
			.Directory               = directory,
			.MaximumDirectoryBytes   = 4096U,
			.MaximumDirectoryEntries = 16U,
			.StandardOutputFile      = spool,
			.DiscardStandardOutput   = true,
		});
		expect(runner != nullptr, "the spooling runner is available");
		const auto result = runner->run(base_request(store, { "--bpr-helper", "stdout", "7" }), { });
		expect(result.has_value(), "a launch whose standard output is a spool file starts under the restriction");
		std::error_code sizeError;
		expect(std::filesystem::file_size(spool, sizeError) == 7U && !sizeError, "the child wrote its standard output into the spool");
		std::cout << "BPR-1114 spooled launch: the child wrote 7 bytes into its spool\n";
	}

	// [[BPR-1114]]. A launch whose handle list cannot be built is refused by
	// process creation itself: nothing is created, so no child runs, and the
	// run fails as start_failure. The seam withholds the standard error handle,
	// as a null standard handle would. The job's accounting shows that no
	// process was created at all, not that one was created and killed, and the
	// child's marker shows that it never ran. The same launch without the seam
	// creates a process and succeeds, so the count can see a launch.
	auto test_windows_refused_launch_creates_nothing() -> void
	{
		QTemporaryDir directory;
		expect(directory.isValid(), "refused-launch directory is available");
		const auto              seam   = std::make_shared<standard_handle_list_seam>();
		const auto              runner = make_github_cli_process_runner_for_test(seam);
		const created_processes created;

		const auto refusedMarker  = directory.filePath(QStringLiteral("refused.marker"));
		const auto createdBefore  = created.total();
		const auto refused        = runner->run(base_request(directory, { "--bpr-helper", "touch", utf8(refusedMarker) }), { });
		const auto createdRefused = created.total() - createdBefore;
		expect(
			!refused.has_value() && refused.error() == github_cli_process_failure::start_failure,
			"a launch refused for its handle list fails as start_failure");
		expect(
			createdRefused == 0U,
			"a launch refused for its handle list creates no process at all (created "
				+ std::to_string(createdRefused) + ")");
		expect(!QFileInfo::exists(refusedMarker), "the refused child never ran");

		seam->Fail.store(false);
		const auto allowedMarker        = directory.filePath(QStringLiteral("allowed.marker"));
		const auto createdAllowedBefore = created.total();
		const auto allowed              = runner->run(base_request(directory, { "--bpr-helper", "touch", utf8(allowedMarker) }), { });
		const auto createdAllowed       = created.total() - createdAllowedBefore;
		expect(allowed.has_value(), "the same launch with its handle list succeeds");
		expect(createdAllowed >= 1U, "the accounting job counts the launch it allowed");
		expect(QFileInfo::exists(allowedMarker), "the allowed child ran");
		std::cout << "BPR-1114 refused launch: start_failure, " << createdRefused << " processes created; the allowed launch created "
				  << createdAllowed << '\n';
	}
#endif
}

auto main(int argc, char* argv[]) -> int
try {
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
#if defined(_WIN32)
		// The helper writes exact bytes. In the C runtime's text mode the
		// `\r\n` of the http-error response came out as `\r\r\n`, and the
		// opt-in row failed on Windows for that alone ([[BPR-0878]] item 3).
		static_cast<void>(::_setmode(::_fileno(stdout), _O_BINARY));
		static_cast<void>(::_setmode(::_fileno(stderr), _O_BINARY));
#endif
		return helper_main(arguments);
	}

	test_exact_process_boundary();
	test_bare_repository_working_directory_is_rejected();
	test_shape_limits_reject_without_launch();
	test_git_acquisition_factory_refuses_unbounded_stores();
	test_git_acquisition_store_sampling();
	test_stream_limits_and_redaction();
	test_sensitive_execution();
#if defined(Q_OS_UNIX)
	test_credentialed_descendant_lifetime();
	test_live_credentialed_quarantine();
#endif
	test_completed_output_opt_in();
#if defined(Q_OS_UNIX)
	test_unix_process_hardening();
	#if defined(Q_OS_MACOS)
	test_unix_descriptor_sweep_is_bounded();
	#endif
	test_descendant_process_group_is_killed();
	test_expired_queued_request_never_launches();
#endif
	test_watchdog_failure_quarantines_worker();
#if defined(Q_OS_UNIX)
	test_unreapable_child_is_abandoned();
#endif
	test_deadline_cancellation_and_startup();
#if defined(_WIN32)
	test_windows_launch_inherits_only_standard_handles();
	test_windows_spooled_launch_keeps_its_file();
	// Last: it moves this process into an accounting job it cannot leave.
	test_windows_refused_launch_creates_nothing();
#endif
	std::cout << "GitHub CLI process runner tests passed\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
