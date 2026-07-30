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

namespace
{
using namespace std::chrono_literals;
using namespace bpr::adapters::github::detail;

static_assert(!std::is_copy_constructible_v<github_cli_process_port>);
static_assert(!std::is_move_constructible_v<github_cli_process_port>);
static_assert(
    std::is_same_v<
        github_cli_process_result::error_type,
        github_cli_process_failure>);

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] auto utf8(const QString& value) -> std::string
{
    const auto bytes = value.toUtf8();
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

[[nodiscard]] auto parse_count(const QString& value) -> std::size_t
{
    bool valid = false;
    const auto parsed = value.toULongLong(&valid);
    if(
        !valid
        || parsed
            > static_cast<qulonglong>(
                std::numeric_limits<std::size_t>::max()))
    {
        return 0;
    }
    return static_cast<std::size_t>(parsed);
}

[[nodiscard]] auto exact_environment_matches(
    const QString& unknown_name,
    const QStringList& arguments) -> bool
{
#if defined(_WIN32)
    const auto environment = QProcessEnvironment::systemEnvironment();
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
            == arguments.at(7)
        && !environment.contains(unknown_name);
#else
    static_cast<void>(arguments);
    std::vector<std::pair<std::string, std::string>> actual;
    for(auto current = ::environ; current != nullptr && *current != nullptr;
        ++current)
    {
        const std::string_view entry{*current};
        const auto separator = entry.find('=');
        if(separator == std::string_view::npos)
        {
            return false;
        }
        actual.emplace_back(
            entry.substr(0U, separator),
            entry.substr(separator + 1U));
    }

    std::vector<std::pair<std::string, std::string>> expected{
        {"BPR_ALLOWED_PROCESS_FACT", "fixed"},
    };
#if defined(Q_OS_MACOS)
    const auto platform_entry = std::ranges::find(
        actual,
        std::pair{
            std::string{"__CF_USER_TEXT_ENCODING"},
            macos_text_encoding_environment()});
    if(platform_entry == actual.end())
    {
        return false;
    }
    actual.erase(platform_entry);
#elif defined(Q_OS_LINUX)
    expected.emplace_back("LC_ALL", "C.UTF-8");
#endif
    std::ranges::sort(actual);
    std::ranges::sort(expected);
    return actual == expected
        && std::ranges::none_of(
            actual,
            [&unknown_name](const auto& entry)
            {
                return entry.first == utf8(unknown_name);
            });
#endif
}

[[nodiscard]] auto helper_main(const QStringList& arguments) -> int
{
    if(arguments.size() < 3)
    {
        return 91;
    }

    const auto mode = arguments.at(2);
    if(mode == QStringLiteral("exact"))
    {
#if defined(_WIN32)
        constexpr auto exact_argument_count = 8;
#else
        constexpr auto exact_argument_count = 7;
#endif
        if(arguments.size() != exact_argument_count)
        {
            return 92;
        }
        if(
            !exact_environment_matches(arguments.at(5), arguments)
            || QDir::current().canonicalPath() != arguments.at(6))
        {
            return 93;
        }

        const std::string input{
            std::istreambuf_iterator<char>{std::cin},
            std::istreambuf_iterator<char>{},
        };
        const std::string expected{"body\0bytes", 10};
        if(
            arguments.at(3) != QStringLiteral("alpha")
            || arguments.at(4) != QStringLiteral("beta")
            || input != expected)
        {
            return 94;
        }
        std::cout << "ok";
        return 0;
    }

    if(
        mode == QStringLiteral("stdout")
        || mode == QStringLiteral("stderr")
        || mode == QStringLiteral("both"))
    {
        if(arguments.size() != 4)
        {
            return 95;
        }
        const auto count = parse_count(arguments.at(3));
        const std::string bytes(count, 'x');
        if(mode == QStringLiteral("stdout"))
        {
            std::cout.write(
                bytes.data(),
                static_cast<std::streamsize>(bytes.size()));
            return 0;
        }
        if(mode == QStringLiteral("stderr"))
        {
            std::cerr.write(
                bytes.data(),
                static_cast<std::streamsize>(bytes.size()));
            return 0;
        }

        std::thread output_writer{
            [&bytes]
            {
                std::cout.write(
                    bytes.data(),
                    static_cast<std::streamsize>(bytes.size()));
                std::cout.flush();
            }};
        std::thread error_writer{
            [&bytes]
            {
                std::cerr.write(
                    bytes.data(),
                    static_cast<std::streamsize>(bytes.size()));
                std::cerr.flush();
            }};
        output_writer.join();
        error_writer.join();
        return 0;
    }

    if(mode == QStringLiteral("hang"))
    {
        std::this_thread::sleep_for(10s);
        return 0;
    }
    if(mode == QStringLiteral("nonzero"))
    {
        std::cout << "discard-me";
        return 7;
    }
    if(mode == QStringLiteral("touch"))
    {
        if(arguments.size() < 4)
        {
            return 102;
        }
        QFile marker{arguments.at(3)};
        if(!marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return 103;
        }
        return marker.write("started", 7) == 7 ? 0 : 104;
    }
    if(mode == QStringLiteral("crash"))
    {
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
    if(mode == QStringLiteral("unix-hardening"))
    {
        if(arguments.size() != 4)
        {
            return 105;
        }
        bool valid_descriptor = false;
        const auto inherited_descriptor =
            arguments.at(3).toInt(&valid_descriptor);
        errno = 0;
        if(
            !valid_descriptor
            || ::fcntl(inherited_descriptor, F_GETFD) != -1
            || errno != EBADF)
        {
            return 106;
        }

        struct sigaction signal_disposition
        {
        };
        if(
            ::sigaction(SIGUSR1, nullptr, &signal_disposition) != 0
            || signal_disposition.sa_handler != SIG_DFL)
        {
            return 107;
        }

        struct rlimit core_limit
        {
        };
        if(
            ::getrlimit(RLIMIT_CORE, &core_limit) != 0
            || core_limit.rlim_cur != 0)
        {
            return 108;
        }
        if(
            ::getsid(0) != ::getpid()
            || ::getpgrp() != ::getpid())
        {
            return 109;
        }
        std::cout << "hardened";
        return 0;
    }

    if(
        mode == QStringLiteral("descendant")
        || mode == QStringLiteral("success-descendant")
        || mode == QStringLiteral("failure-descendant")
        || mode == QStringLiteral("escaped-group-descendant"))
    {
        if(arguments.size() != 4)
        {
            return 98;
        }
        const bool clean_success =
            mode == QStringLiteral("success-descendant");
        const bool fixed_failure =
            mode == QStringLiteral("failure-descendant");
        const bool escaped_group =
            mode == QStringLiteral("escaped-group-descendant");
        const auto descendant = ::fork();
        if(descendant < 0)
        {
            return 99;
        }
        if(descendant == 0)
        {
            if(escaped_group && ::setpgid(0, 0) != 0)
            {
                return 110;
            }
            if(clean_success || fixed_failure || escaped_group)
            {
                static_cast<void>(::close(STDIN_FILENO));
                static_cast<void>(::close(STDOUT_FILENO));
                static_cast<void>(::close(STDERR_FILENO));
            }
            for(;;)
            {
                static_cast<void>(::pause());
            }
        }

        if(
            escaped_group
            && ::setpgid(descendant, descendant) != 0)
        {
            static_cast<void>(::kill(descendant, SIGKILL));
            return 111;
        }
        QFile pid_file{arguments.at(3)};
        if(!pid_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            static_cast<void>(::kill(descendant, SIGKILL));
            return 100;
        }
        const auto bytes = QByteArray::number(
            static_cast<qlonglong>(descendant));
        if(pid_file.write(bytes) != bytes.size() || !pid_file.flush())
        {
            static_cast<void>(::kill(descendant, SIGKILL));
            return 101;
        }
        pid_file.close();
        if(clean_success || escaped_group)
        {
            std::cout << "clean";
            return 0;
        }
        if(fixed_failure)
        {
            std::cout << "discard-me";
            return 7;
        }
        for(;;)
        {
            static_cast<void>(::pause());
        }
    }
#endif
    return 96;
}

[[nodiscard]] auto canonical_application_path() -> std::string
{
    return utf8(
        QFileInfo{QCoreApplication::applicationFilePath()}
            .canonicalFilePath());
}

[[nodiscard]] auto canonical_directory(const QTemporaryDir& directory)
    -> std::string
{
    return utf8(QFileInfo{directory.path()}.canonicalFilePath());
}

#if defined(_WIN32)
[[nodiscard]] auto windows_system_root() -> std::string
{
    wchar_t buffer[MAX_PATH + 1]{};
    const auto length = GetWindowsDirectoryW(buffer, MAX_PATH + 1);
    expect(
        length != 0U && length < MAX_PATH + 1U,
        "Windows system root is obtained without ambient environment");
    return utf8(
        QFileInfo{
            QString::fromWCharArray(
                buffer,
                static_cast<qsizetype>(length))}
            .canonicalFilePath());
}
#endif

[[nodiscard]] auto base_request(
    const QTemporaryDir& directory,
    std::vector<std::string> arguments) -> github_cli_process_request
{
    std::vector<github_cli_process_environment_entry> environment{
        {
            .Name = "BPR_ALLOWED_PROCESS_FACT",
            .Value = "fixed",
        },
    };
#if defined(Q_OS_LINUX)
    environment.push_back({
        .Name = "LC_ALL",
        .Value = "C.UTF-8",
    });
#elif defined(_WIN32)
    environment.push_back({
        .Name = "PATH",
        .Value = "",
    });
    environment.push_back({
        .Name = "SystemRoot",
        .Value = windows_system_root(),
    });
#endif
    return {
        .Executable = canonical_application_path(),
        .Arguments = std::move(arguments),
        .Environment = std::move(environment),
        .WorkingDirectory = canonical_directory(directory),
        .StandardInput = {},
        .StandardInputLimit = 64U,
        .StandardOutputLimit = 64U,
        .StandardErrorLimit = 64U,
        .Timeout = 2s,
        .ReapTimeout = 1s,
    };
}

auto expect_failure(
    github_cli_process_port& runner,
    const github_cli_process_request& request,
    const github_cli_process_failure expected,
    const std::string_view message) -> void
{
    const auto result = runner.run(request, {});
    expect(!result.has_value() && result.error() == expected, message);
}

auto test_exact_process_boundary() -> void
{
    QTemporaryDir directory;
    expect(directory.isValid(), "neutral process directory is available");

    const auto suffix = std::to_string(
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count());
    const auto unknown_name = QByteArray::fromStdString(
        "BPR_UNKNOWN_PROCESS_CANARY_" + suffix);
    const auto unknown_value =
        QByteArray::fromStdString("runtime-only-" + suffix);
    expect(
        qputenv(unknown_name.constData(), unknown_value),
        "unknown parent canary is installed");

    auto request = base_request(
        directory,
        {
            "--bpr-helper",
            "exact",
            "alpha",
            "beta",
            unknown_name.toStdString(),
            canonical_directory(directory),
#if defined(_WIN32)
            windows_system_root(),
#endif
        });
    request.StandardInput = std::string{"body\0bytes", 10};
    request.StandardInputLimit = request.StandardInput.size();
    request.StandardOutputLimit = 2U;

    const auto runner = make_github_cli_process_runner();
    const auto result = runner->run(request, {});
    static_cast<void>(qunsetenv(unknown_name.constData()));

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
    QDir root{repository.path()};
    expect(
        root.mkdir(QStringLiteral("objects"))
            && root.mkdir(QStringLiteral("refs"))
            && root.mkdir(QStringLiteral("nested")),
        "bare repository fixture directories are created");

    QFile head{root.filePath(QStringLiteral("HEAD"))};
    expect(head.open(QIODevice::WriteOnly), "bare repository HEAD is created");
    expect(
        head.write("ref: refs/heads/main\n", 21) == 21,
        "bare repository HEAD is populated");
    head.close();

    auto request =
        base_request(repository, {"--bpr-helper", "stdout", "2"});
    request.WorkingDirectory = utf8(
        QFileInfo{root.filePath(QStringLiteral("nested"))}
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
    const auto marker =
        directory.filePath(QStringLiteral("shape-limit.marker"));

    const auto touch_request = [&]
    {
        return base_request(
            directory,
            {
                "--bpr-helper",
                "touch",
                utf8(marker),
            });
    };
    const auto expect_rejected =
        [&](const github_cli_process_request& request,
            const std::string_view message)
    {
        static_cast<void>(QFile::remove(marker));
        expect_failure(
            *runner,
            request,
            github_cli_process_failure::invalid_request,
            message);
        expect(
            !QFileInfo{marker}.exists(),
            "shape-invalid request never launches its child");
    };

    auto request = touch_request();
    request.Executable.assign(
        github_cli_process_max_path_bytes + 1U,
        'x');
    expect_rejected(request, "executable path N+1 is rejected");

    request = touch_request();
    request.Executable.push_back('\0');
    expect_rejected(request, "executable path NUL is rejected");

    request = touch_request();
    request.WorkingDirectory.assign(
        github_cli_process_max_path_bytes + 1U,
        'x');
    expect_rejected(request, "working directory path N+1 is rejected");

    request = touch_request();
    request.WorkingDirectory.push_back('\0');
    expect_rejected(request, "working directory path NUL is rejected");

    request = touch_request();
    request.Arguments.push_back(std::string(
        github_cli_process_max_argument_bytes + 1U,
        'x'));
    expect_rejected(request, "argument size N+1 is rejected");

    request = touch_request();
    while(
        request.Arguments.size()
        <= github_cli_process_max_argument_count)
    {
        request.Arguments.emplace_back("x");
    }
    expect_rejected(request, "argument count N+1 is rejected");

    request = touch_request();
    for(std::size_t index = 0; index < 3U; ++index)
    {
        request.Arguments.push_back(std::string(
            github_cli_process_max_argument_bytes,
            'x'));
    }
    expect_rejected(request, "aggregate argument bytes N+1 are rejected");

    request = touch_request();
    request.Arguments.push_back(std::string{"x\0y", 3U});
    expect_rejected(request, "argument NUL is rejected");

    request = touch_request();
    while(
        request.Environment.size()
        <= github_cli_process_max_environment_count)
    {
        request.Environment.push_back({
            .Name =
                "BPR_SHAPE_" + std::to_string(request.Environment.size()),
            .Value = "x",
        });
    }
    expect_rejected(request, "environment count N+1 is rejected");

    request = touch_request();
    request.Environment.push_back({
        .Name = std::string(
            github_cli_process_max_environment_name_bytes + 1U,
            'A'),
        .Value = "x",
    });
    expect_rejected(request, "environment name size N+1 is rejected");

    request = touch_request();
    request.Environment.push_back({
        .Name = "BPR_SHAPE_VALUE",
        .Value = std::string(
            github_cli_process_max_environment_value_bytes + 1U,
            'x'),
    });
    expect_rejected(request, "environment value size N+1 is rejected");

    request = touch_request();
    for(std::size_t index = 0; index < 3U; ++index)
    {
        request.Environment.push_back({
            .Name = "BPR_SHAPE_AGGREGATE_" + std::to_string(index),
            .Value = std::string(
                github_cli_process_max_environment_value_bytes,
                'x'),
        });
    }
    expect_rejected(request, "aggregate environment bytes N+1 are rejected");

    request = touch_request();
    request.Environment.push_back(
        request.Environment.front());
    expect_rejected(request, "duplicate environment names are rejected");

    request = touch_request();
    request.StandardInputLimit =
        github_cli_process_max_standard_input_bytes + 1U;
    expect_rejected(request, "stdin limit N+1 is rejected");

    request = touch_request();
    request.StandardInputLimit =
        github_cli_process_max_standard_input_bytes;
    request.StandardInput.assign(
        github_cli_process_max_standard_input_bytes + 1U,
        'x');
    expect_rejected(request, "stdin payload N+1 is rejected");

    request = touch_request();
    request.StandardOutputLimit =
        github_cli_process_max_standard_output_bytes + 1U;
    expect_rejected(request, "stdout limit N+1 is rejected");

    request = touch_request();
    request.StandardErrorLimit =
        github_cli_process_max_standard_error_bytes + 1U;
    expect_rejected(request, "stderr limit N+1 is rejected");

    request = touch_request();
    request.Timeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            github_cli_process_max_timeout)
        + 1ms;
    expect_rejected(request, "process timeout N+1 is rejected");

    request = touch_request();
    request.ReapTimeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            github_cli_process_max_reap_timeout)
        + 1ms;
    expect_rejected(request, "reap timeout N+1 is rejected");
}

auto test_stream_limits_and_redaction() -> void
{
    QTemporaryDir directory;
    expect(directory.isValid(), "stream test directory is available");
    const auto runner = make_github_cli_process_runner();

    auto request =
        base_request(directory, {"--bpr-helper", "stdout", "31"});
    request.StandardOutputLimit = 31U;
    auto result = runner->run(request, {});
    expect(
        result.has_value() && result->StandardOutput.size() == 31U,
        "stdout at N bytes succeeds");

    request.Arguments.back() = "32";
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::standard_output_limit,
        "stdout at N+1 bytes fails atomically");

    request =
        base_request(directory, {"--bpr-helper", "stderr", "31"});
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

    request =
        base_request(directory, {"--bpr-helper", "both", "131072"});
    request.StandardOutputLimit = 1024U;
    request.StandardErrorLimit = 1024U;
    result = runner->run(request, {});
    expect(
        !result.has_value()
            && (
                result.error()
                    == github_cli_process_failure::standard_output_limit
                || result.error()
                    == github_cli_process_failure::standard_error_limit),
        "simultaneous stdout and stderr flood is drained and bounded");

    request =
        base_request(directory, {"--bpr-helper", "nonzero"});
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::nonzero_exit,
        "nonzero exit exposes no partial stdout");

    request = base_request(directory, {"--bpr-helper", "crash"});
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::process_crash,
        "crash exposes no partial stdout");
}

#if defined(Q_OS_UNIX)
[[nodiscard]] auto read_descendant_pid(const QString& path) -> pid_t
{
    QFile file{path};
    expect(file.open(QIODevice::ReadOnly), "descendant pid file is readable");
    bool valid = false;
    const auto value = file.readAll().trimmed().toLongLong(&valid);
    expect(
        valid && value > 1
            && value
                <= static_cast<qlonglong>(
                    std::numeric_limits<pid_t>::max()),
        "descendant pid is bounded and valid");
    return static_cast<pid_t>(value);
}

[[nodiscard]] auto wait_until_process_is_gone(const pid_t process) -> bool
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while(std::chrono::steady_clock::now() < deadline)
    {
        errno = 0;
        if(::kill(process, 0) == -1 && errno == ESRCH)
        {
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

    const auto inherited_descriptor =
        ::open("/dev/null", O_RDONLY);
    expect(
        inherited_descriptor >= 3,
        "non-standard inherited descriptor is available");
    const auto descriptor_flags =
        ::fcntl(inherited_descriptor, F_GETFD);
    expect(
        descriptor_flags >= 0
            && ::fcntl(
                   inherited_descriptor,
                   F_SETFD,
                   descriptor_flags & ~FD_CLOEXEC)
                == 0,
        "inherited descriptor close-on-exec is explicitly disabled");

    struct sigaction prior_disposition
    {
    };
    struct sigaction ignored_disposition
    {
    };
    ignored_disposition.sa_handler = SIG_IGN;
    expect(
        sigemptyset(&ignored_disposition.sa_mask) == 0
            && ::sigaction(
                   SIGUSR1,
                   &ignored_disposition,
                   &prior_disposition)
                == 0,
        "parent signal disposition is changed for reset verification");

    struct rlimit prior_core_limit
    {
    };
    expect(
        ::getrlimit(RLIMIT_CORE, &prior_core_limit) == 0,
        "parent core limit is available");
    const auto canary_core_limit =
        prior_core_limit.rlim_max == RLIM_INFINITY
        ? static_cast<rlim_t>(1)
        : std::min<rlim_t>(
              prior_core_limit.rlim_max,
              static_cast<rlim_t>(1));
    expect(
        canary_core_limit > 0,
        "parent permits a nonzero core-limit canary");
    auto raised_core_limit = prior_core_limit;
    raised_core_limit.rlim_cur = canary_core_limit;
    expect(
        ::setrlimit(RLIMIT_CORE, &raised_core_limit) == 0,
        "parent core limit is raised for disable verification");

    auto request = base_request(
        directory,
        {
            "--bpr-helper",
            "unix-hardening",
            std::to_string(inherited_descriptor),
        });
    request.StandardOutputLimit = 8U;
    const auto result = runner->run(request, {});

    const bool core_limit_restored =
        ::setrlimit(RLIMIT_CORE, &prior_core_limit) == 0;
    const bool signal_restored =
        ::sigaction(SIGUSR1, &prior_disposition, nullptr) == 0;
    const bool descriptor_closed =
        ::close(inherited_descriptor) == 0;
    expect(core_limit_restored, "parent core limit is restored");
    expect(signal_restored, "parent signal disposition is restored");
    expect(descriptor_closed, "parent test descriptor is closed");
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

    const auto success_pid_file =
        directory.filePath(QStringLiteral("success-descendant.pid"));
    auto request = base_request(
        directory,
        {
            "--bpr-helper",
            "success-descendant",
            utf8(success_pid_file),
        });
    request.StandardOutputLimit = 5U;
    const auto success = runner->run(request, {});
    expect(
        success.has_value() && success->StandardOutput == "clean",
        "clean leader exit retains exact successful output");
    const auto success_descendant =
        read_descendant_pid(success_pid_file);
    const bool success_descendant_gone =
        wait_until_process_is_gone(success_descendant);
    if(!success_descendant_gone)
    {
        static_cast<void>(::kill(success_descendant, SIGKILL));
    }
    expect(
        success_descendant_gone,
        "clean leader exit kills its remaining process-group descendant");

    const auto failure_pid_file =
        directory.filePath(QStringLiteral("failure-descendant.pid"));
    request = base_request(
        directory,
        {
            "--bpr-helper",
            "failure-descendant",
            utf8(failure_pid_file),
        });
    request.StandardOutputLimit = 10U;
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::nonzero_exit,
        "nonzero leader exit exposes no output and returns a fixed failure");
    const auto failure_descendant =
        read_descendant_pid(failure_pid_file);
    const bool failure_descendant_gone =
        wait_until_process_is_gone(failure_descendant);
    if(!failure_descendant_gone)
    {
        static_cast<void>(::kill(failure_descendant, SIGKILL));
    }
    expect(
        failure_descendant_gone,
        "nonzero leader exit kills its remaining process-group descendant");

    const auto timeout_pid_file =
        directory.filePath(QStringLiteral("timeout-descendant.pid"));
    request = base_request(
        directory,
        {
            "--bpr-helper",
            "descendant",
            utf8(timeout_pid_file),
        });
    request.Timeout = 500ms;
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::timeout,
        "timeout kills and reaps the process-group leader");
    const auto timeout_descendant =
        read_descendant_pid(timeout_pid_file);
    const bool timeout_descendant_gone =
        wait_until_process_is_gone(timeout_descendant);
    if(!timeout_descendant_gone)
    {
        static_cast<void>(::kill(timeout_descendant, SIGKILL));
    }
    expect(
        timeout_descendant_gone,
        "timeout kills descendants remaining in the initial process group");

    const auto cancelled_pid_file =
        directory.filePath(QStringLiteral("cancelled-descendant.pid"));
    request = base_request(
        directory,
        {
            "--bpr-helper",
            "descendant",
            utf8(cancelled_pid_file),
        });
    std::stop_source cancellation;
    std::thread canceller{
        [&]
        {
            const auto deadline =
                std::chrono::steady_clock::now() + 1s;
            while(
                std::chrono::steady_clock::now() < deadline
                && QFileInfo{cancelled_pid_file}.size() <= 0)
            {
                std::this_thread::sleep_for(10ms);
            }
            cancellation.request_stop();
        }};
    const auto cancelled =
        runner->run(request, cancellation.get_token());
    canceller.join();
    expect(
        !cancelled.has_value()
            && cancelled.error()
                == github_cli_process_failure::cancelled,
        "cancellation kills and reaps the process-group leader");
    const auto cancelled_descendant =
        read_descendant_pid(cancelled_pid_file);
    const bool cancelled_descendant_gone =
        wait_until_process_is_gone(cancelled_descendant);
    if(!cancelled_descendant_gone)
    {
        static_cast<void>(::kill(cancelled_descendant, SIGKILL));
    }
    expect(
        cancelled_descendant_gone,
        "cancellation kills descendants remaining in the initial "
        "process group");

    const auto escaped_pid_file =
        directory.filePath(QStringLiteral("escaped-group-descendant.pid"));
    request = base_request(
        directory,
        {
            "--bpr-helper",
            "escaped-group-descendant",
            utf8(escaped_pid_file),
        });
    request.StandardOutputLimit = 5U;
    const auto escaped_result = runner->run(request, {});
    expect(
        escaped_result.has_value()
            && escaped_result->StandardOutput == "clean",
        "leader succeeds after its descendant moves to another group");
    const auto escaped_descendant =
        read_descendant_pid(escaped_pid_file);
    errno = 0;
    const bool escaped_descendant_survived =
        ::kill(escaped_descendant, 0) == 0
        || errno == EPERM;
    static_cast<void>(::kill(escaped_descendant, SIGKILL));
    const bool escaped_descendant_cleaned =
        wait_until_process_is_gone(escaped_descendant);
    expect(
        escaped_descendant_survived,
        "cleanup guarantee intentionally excludes descendants that leave "
        "the initial process group");
    expect(
        escaped_descendant_cleaned,
        "escaped-group fixture performs explicit same-test cleanup");
}

auto test_expired_queued_request_never_launches() -> void
{
    QTemporaryDir directory;
    expect(directory.isValid(), "queued test directory is available");
    const auto runner = make_github_cli_process_runner();

    const auto blocker_pid_file =
        directory.filePath(QStringLiteral("blocker-descendant.pid"));
    auto blocker_request = base_request(
        directory,
        {
            "--bpr-helper",
            "descendant",
            utf8(blocker_pid_file),
        });
    blocker_request.Timeout = 2s;

    std::stop_source blocker_cancellation;
    std::optional<github_cli_process_result> blocker_result;
    std::thread blocker{
        [&]
        {
            blocker_result.emplace(runner->run(
                blocker_request,
                blocker_cancellation.get_token()));
        }};

    const auto ready_deadline =
        std::chrono::steady_clock::now() + 1s;
    while(
        std::chrono::steady_clock::now() < ready_deadline
        && QFileInfo{blocker_pid_file}.size() <= 0)
    {
        std::this_thread::sleep_for(10ms);
    }
    const bool blocker_started =
        QFileInfo{blocker_pid_file}.size() > 0;
    if(!blocker_started)
    {
        blocker_cancellation.request_stop();
        blocker.join();
    }
    expect(blocker_started, "blocking process starts before queue test");

    const auto marker =
        directory.filePath(QStringLiteral("expired-request.marker"));
    auto expired_request = base_request(
        directory,
        {
            "--bpr-helper",
            "touch",
            utf8(marker),
        });
    expired_request.Timeout = 50ms;
    expect_failure(
        *runner,
        expired_request,
        github_cli_process_failure::timeout,
        "queued request expires without entering the process worker");

    blocker_cancellation.request_stop();
    blocker.join();
    expect(
        blocker_result.has_value()
            && !blocker_result->has_value()
            && blocker_result->error()
                == github_cli_process_failure::cancelled,
        "blocking process is cancelled after queue expiry");
    std::this_thread::sleep_for(50ms);
    expect(
        !QFileInfo{marker}.exists(),
        "expired queued request never launches later");

    const auto descendant = read_descendant_pid(blocker_pid_file);
    const bool descendant_gone =
        wait_until_process_is_gone(descendant);
    if(!descendant_gone)
    {
        static_cast<void>(::kill(descendant, SIGKILL));
    }
    expect(
        descendant_gone,
        "queue blocker descendant is contained during cancellation");
}
#endif

class blocking_process_runner_test_seam final
    : public github_cli_process_runner_test_seam
{
public:
    auto before_execute() -> void override
    {
        std::unique_lock lock{Mutex};
        Entered = true;
        StateChanged.notify_all();
        StateChanged.wait(
            lock,
            [&]
            {
                return Released;
            });
    }

    [[nodiscard]] auto wait_until_entered() -> bool
    {
        std::unique_lock lock{Mutex};
        return StateChanged.wait_for(
            lock,
            1s,
            [&]
            {
                return Entered;
            });
    }

    auto release() -> void
    {
        {
            std::lock_guard lock{Mutex};
            Released = true;
        }
        StateChanged.notify_all();
    }

private:
    std::mutex Mutex;
    std::condition_variable StateChanged;
    bool Entered = false;
    bool Released = false;
};

auto test_watchdog_failure_quarantines_worker() -> void
{
    QTemporaryDir directory;
    expect(directory.isValid(), "watchdog test directory is available");
    const auto test_seam =
        std::make_shared<blocking_process_runner_test_seam>();
    const auto runner =
        make_github_cli_process_runner_for_test(test_seam);

    const auto blocked_marker =
        directory.filePath(QStringLiteral("blocked.marker"));
    auto blocked_request = base_request(
        directory,
        {
            "--bpr-helper",
            "touch",
            utf8(blocked_marker),
        });
    blocked_request.Timeout = 20ms;
    blocked_request.ReapTimeout = 1ms;

    std::optional<github_cli_process_result> blocked_result;
    std::thread blocked_caller{
        [&]
        {
            blocked_result.emplace(
                runner->run(blocked_request, {}));
        }};
    const bool worker_stalled =
        test_seam->wait_until_entered();
    expect(
        worker_stalled,
        "test seam stalls the sole worker before execution");
    blocked_caller.join();
    expect(
        blocked_result.has_value()
            && !blocked_result->has_value()
            && blocked_result->error()
                == github_cli_process_failure::reap_failure,
        "completion watchdog returns a fixed reap failure");
    expect(
        !QFileInfo{blocked_marker}.exists(),
        "watchdog-expired request has not launched");

    const auto quarantined_marker =
        directory.filePath(QStringLiteral("quarantined.marker"));
    auto quarantined_request = base_request(
        directory,
        {
            "--bpr-helper",
            "touch",
            utf8(quarantined_marker),
        });
    const auto quarantined_result =
        runner->run(quarantined_request, {});
    expect(
        !quarantined_result.has_value()
            && quarantined_result.error()
                == github_cli_process_failure::reap_failure,
        "a second caller observes the quarantined busy worker");
    expect(
        !QFileInfo{quarantined_marker}.exists(),
        "quarantined request never launches");

    test_seam->release();
    const auto recovered_marker =
        directory.filePath(QStringLiteral("recovered.marker"));
    auto recovered_request = base_request(
        directory,
        {
            "--bpr-helper",
            "touch",
            utf8(recovered_marker),
        });
    github_cli_process_result recovered =
        std::unexpected{
            github_cli_process_failure::reap_failure};
    const auto recovery_deadline =
        std::chrono::steady_clock::now() + 2s;
    while(
        std::chrono::steady_clock::now() < recovery_deadline
        && !recovered.has_value())
    {
        recovered = runner->run(recovered_request, {});
        if(
            !recovered.has_value()
            && recovered.error()
                == github_cli_process_failure::reap_failure)
        {
            std::this_thread::sleep_for(10ms);
            continue;
        }
        break;
    }
    expect(
        recovered.has_value()
            && QFileInfo{recovered_marker}.exists(),
        "worker reopens only after the stalled execution reaches terminal");
}

auto test_deadline_cancellation_and_startup() -> void
{
    QTemporaryDir directory;
    expect(directory.isValid(), "lifecycle test directory is available");
    const auto runner = make_github_cli_process_runner();

    auto request = base_request(directory, {"--bpr-helper", "hang"});
    request.Timeout = 50ms;
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::timeout,
        "total process deadline kills and reaps the child");

    request.Timeout = 2s;
    std::stop_source cancellation;
    std::thread canceller{
        [&cancellation]
        {
            std::this_thread::sleep_for(50ms);
            cancellation.request_stop();
        }};
    const auto cancelled = runner->run(request, cancellation.get_token());
    canceller.join();
    expect(
        !cancelled.has_value()
            && cancelled.error()
                == github_cli_process_failure::cancelled,
        "stop token cancellation kills and reaps the child");

#if defined(_WIN32)
    const auto invalid_child_name = QStringLiteral("invalid-child.exe");
#else
    const auto invalid_child_name = QStringLiteral("invalid-child");
#endif
    const auto invalid_executable =
        directory.filePath(invalid_child_name);
    QFile file{invalid_executable};
    expect(file.open(QIODevice::WriteOnly), "invalid executable is created");
    expect(file.write("not-an-executable", 17) == 17, "invalid image is written");
    file.close();
    expect(
        QFile::setPermissions(
            invalid_executable,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner),
        "invalid image is executable");

    request = base_request(directory, {"ignored"});
    request.Executable =
        utf8(QFileInfo{invalid_executable}.canonicalFilePath());
    expect_failure(
        *runner,
        request,
        github_cli_process_failure::start_failure,
        "executable image startup failure is fixed and redacted");

    request = base_request(directory, {"--bpr-helper", "hang"});
    std::stop_source already_cancelled;
    already_cancelled.request_stop();
    const auto pre_cancelled =
        runner->run(request, already_cancelled.get_token());
    expect(
        !pre_cancelled.has_value()
            && pre_cancelled.error()
                == github_cli_process_failure::cancelled,
        "pre-start cancellation launches no child");
}
}

auto main(int argc, char* argv[]) -> int
{
#if defined(Q_OS_UNIX)
    if(
        argc >= 2
        && std::string_view{argv[1]} == "--bpr-helper")
    {
        QStringList arguments;
        arguments.reserve(static_cast<qsizetype>(argc));
        for(int index = 0; index < argc; ++index)
        {
            arguments.push_back(QString::fromUtf8(argv[index]));
        }
        return helper_main(arguments);
    }
#endif

    QCoreApplication application{argc, argv};
    const auto arguments = application.arguments();
    if(
        arguments.size() >= 2
        && arguments.at(1) == QStringLiteral("--bpr-helper"))
    {
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
