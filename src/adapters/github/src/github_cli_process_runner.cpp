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
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace bpr::adapters::github::detail
{
namespace
{
using clock = std::chrono::steady_clock;

inline constexpr auto wait_slice = std::chrono::milliseconds{10};

[[nodiscard]] auto contains_nul(const std::string_view value) noexcept -> bool
{
    return value.find('\0') != std::string_view::npos;
}

[[nodiscard]] auto valid_environment_name(
    const std::string_view name) noexcept -> bool
{
    if(name.empty() || contains_nul(name))
    {
        return false;
    }

    const auto ascii_alpha = [](const char value) noexcept
    {
        return (value >= 'A' && value <= 'Z')
            || (value >= 'a' && value <= 'z');
    };
    const auto ascii_digit = [](const char value) noexcept
    {
        return value >= '0' && value <= '9';
    };

    if(name.front() != '_' && !ascii_alpha(name.front()))
    {
        return false;
    }
    return std::all_of(
        name.begin() + 1,
        name.end(),
        [&](const char value)
        {
            return value == '_' || ascii_alpha(value)
                || ascii_digit(value);
        });
}

[[nodiscard]] auto equal_environment_names(
    const std::string_view left,
    const std::string_view right) noexcept -> bool
{
    if(left.size() != right.size())
    {
        return false;
    }

    const auto fold_ascii = [](const char value) noexcept
    {
        if(value >= 'A' && value <= 'Z')
        {
            return static_cast<char>(value - 'A' + 'a');
        }
        return value;
    };
    for(std::size_t index = 0; index < left.size(); ++index)
    {
        if(fold_ascii(left[index]) != fold_ascii(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto add_bounded(
    std::size_t& total,
    const std::size_t amount,
    const std::size_t limit) noexcept -> bool
{
    if(total > limit || amount > limit - total)
    {
        return false;
    }
    total += amount;
    return true;
}

[[nodiscard]] auto valid_request_shape(
    const github_cli_process_request& request) noexcept -> bool
{
    if(
        request.Executable.empty()
        || request.Executable.size() > github_cli_process_max_path_bytes
        || contains_nul(request.Executable)
        || request.WorkingDirectory.empty()
        || request.WorkingDirectory.size()
            > github_cli_process_max_path_bytes
        || contains_nul(request.WorkingDirectory)
        || request.Arguments.empty()
        || request.Arguments.size()
            > github_cli_process_max_argument_count
        || request.Environment.size()
            > github_cli_process_max_environment_count
        || request.StandardInputLimit == 0
        || request.StandardInputLimit
            > github_cli_process_max_standard_input_bytes
        || request.StandardInput.size() > request.StandardInputLimit
        || request.StandardOutputLimit == 0
        || request.StandardOutputLimit
            > github_cli_process_max_standard_output_bytes
        || request.StandardErrorLimit == 0
        || request.StandardErrorLimit
            > github_cli_process_max_standard_error_bytes
        || request.Timeout <= std::chrono::milliseconds::zero()
        || request.Timeout
            > std::chrono::duration_cast<std::chrono::milliseconds>(
                github_cli_process_max_timeout)
        || request.ReapTimeout <= std::chrono::milliseconds::zero()
        || request.ReapTimeout
            > std::chrono::duration_cast<std::chrono::milliseconds>(
                github_cli_process_max_reap_timeout))
    {
        return false;
    }

    std::size_t argument_bytes = 0;
    for(const auto& argument : request.Arguments)
    {
        if(
            argument.empty()
            || argument.size() > github_cli_process_max_argument_bytes
            || contains_nul(argument)
            || !add_bounded(
                argument_bytes,
                argument.size(),
                github_cli_process_max_arguments_bytes))
        {
            return false;
        }
    }

    std::size_t environment_bytes = 0;
#if defined(_WIN32)
    bool has_empty_path = false;
    bool has_system_root = false;
#endif
    for(std::size_t index = 0; index < request.Environment.size(); ++index)
    {
        const auto& entry = request.Environment[index];
        if(
            entry.Name.size()
                > github_cli_process_max_environment_name_bytes
            || !valid_environment_name(entry.Name)
            || entry.Value.size()
                > github_cli_process_max_environment_value_bytes
            || contains_nul(entry.Value)
            || !add_bounded(
                environment_bytes,
                entry.Name.size(),
                github_cli_process_max_environment_bytes)
            || !add_bounded(
                environment_bytes,
                entry.Value.size(),
                github_cli_process_max_environment_bytes))
        {
            return false;
        }
#if defined(_WIN32)
        if(equal_environment_names(entry.Name, "PATH"))
        {
            has_empty_path = entry.Value.empty();
        }
        if(equal_environment_names(entry.Name, "SystemRoot"))
        {
            has_system_root = !entry.Value.empty();
        }
#endif
        for(std::size_t prior = 0; prior < index; ++prior)
        {
            if(equal_environment_names(
                   entry.Name,
                   request.Environment[prior].Name))
            {
                return false;
            }
        }
    }
#if defined(_WIN32)
    if(!has_empty_path || !has_system_root)
    {
        return false;
    }
#endif
    return true;
}

[[nodiscard]] auto decode_utf8(
    const std::string_view value,
    QString& decoded) -> bool
{
    if(value.size()
       > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max()))
    {
        return false;
    }

    decoded = QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size()));
    const auto encoded = decoded.toUtf8();
    return static_cast<std::size_t>(encoded.size()) == value.size()
        && std::equal(
            value.begin(),
            value.end(),
            encoded.constData());
}

[[nodiscard]] auto exact_canonical_file(
    const std::string_view path,
    QString& canonical) -> bool
{
    QString decoded;
    if(
        path.empty() || path.size() > github_cli_process_max_path_bytes
        || contains_nul(path) || !decode_utf8(path, decoded))
    {
        return false;
    }

    const QFileInfo information{decoded};
    canonical = information.canonicalFilePath();
    return information.isAbsolute() && information.exists()
        && information.isFile() && information.isExecutable()
        && !canonical.isEmpty()
        && QDir::cleanPath(QDir::fromNativeSeparators(decoded))
            == QDir::cleanPath(QDir::fromNativeSeparators(canonical));
}

[[nodiscard]] auto exact_canonical_neutral_directory(
    const std::string_view path,
    QString& canonical) -> bool
{
    QString decoded;
    if(
        path.empty() || path.size() > github_cli_process_max_path_bytes
        || contains_nul(path) || !decode_utf8(path, decoded))
    {
        return false;
    }

    const QFileInfo information{decoded};
    canonical = information.canonicalFilePath();
    if(
        !information.isAbsolute() || !information.exists()
        || !information.isDir() || canonical.isEmpty()
        || QDir::cleanPath(QDir::fromNativeSeparators(decoded))
            != QDir::cleanPath(QDir::fromNativeSeparators(canonical)))
    {
        return false;
    }

    QDir directory{canonical};
    for(;;)
    {
        const QFileInfo dot_git{
            directory.filePath(QStringLiteral(".git"))};
        const QFileInfo head{
            directory.filePath(QStringLiteral("HEAD"))};
        const QFileInfo objects{
            directory.filePath(QStringLiteral("objects"))};
        const QFileInfo refs{
            directory.filePath(QStringLiteral("refs"))};
        const bool bare_repository =
            head.isFile() && objects.isDir() && refs.isDir();
        if(dot_git.exists() || bare_repository)
        {
            return false;
        }

        const auto current = directory.absolutePath();
        if(
            !directory.cdUp()
            || directory.absolutePath() == current)
        {
            break;
        }
    }
    return true;
}

struct validated_process_request
{
    QString Executable;
    QStringList Arguments;
    QProcessEnvironment Environment;
    QString WorkingDirectory;
};

[[nodiscard]] auto validate_request(
    const github_cli_process_request& request,
    validated_process_request& validated) -> bool
{
    if(
        !valid_request_shape(request)
        || !exact_canonical_file(
            request.Executable,
            validated.Executable)
        || !exact_canonical_neutral_directory(
            request.WorkingDirectory,
            validated.WorkingDirectory))
    {
        return false;
    }

    for(const auto& argument : request.Arguments)
    {
        QString decoded;
        if(!decode_utf8(argument, decoded))
        {
            return false;
        }
        validated.Arguments.push_back(std::move(decoded));
    }

    for(const auto& entry : request.Environment)
    {
        QString name;
        QString value;
        if(!decode_utf8(entry.Name, name) || !decode_utf8(entry.Value, value))
        {
            return false;
        }
        validated.Environment.insert(name, value);
    }
    return true;
}

[[nodiscard]] auto wait_milliseconds(
    const clock::time_point deadline) noexcept -> int
{
    const auto remaining = deadline - clock::now();
    if(remaining <= clock::duration::zero())
    {
        return 0;
    }

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
    if(milliseconds < std::chrono::milliseconds{1})
    {
        return 1;
    }
    return static_cast<int>(std::min(milliseconds, wait_slice).count());
}

auto kill_process_group(const qint64 process_group_id) noexcept -> void
{
#if defined(Q_OS_UNIX)
    if(
        process_group_id > 1
        && process_group_id
            <= static_cast<qint64>(
                std::numeric_limits<pid_t>::max()))
    {
        const auto process_group =
            -static_cast<pid_t>(process_group_id);
        static_cast<void>(::kill(process_group, SIGKILL));
    }
#else
    static_cast<void>(process_group_id);
#endif
}

[[nodiscard]] auto resolve_process_group(
    const QProcess& process,
    const qint64 known_process_group_id) noexcept -> qint64
{
    if(known_process_group_id > 1)
    {
        return known_process_group_id;
    }
    const auto process_id = process.processId();
    return process_id > 1 ? process_id : 0;
}

[[nodiscard]] auto kill_process_group_and_reap(
    QProcess& process,
    const qint64 known_process_group_id,
    const std::chrono::milliseconds timeout) -> bool
{
    const auto deadline = clock::now() + timeout;
    auto process_group_id =
        resolve_process_group(process, known_process_group_id);
    kill_process_group(process_group_id);
    if(process.state() != QProcess::NotRunning)
    {
        process.kill();
    }

    while(process.state() != QProcess::NotRunning)
    {
        const auto wait = wait_milliseconds(deadline);
        if(wait == 0)
        {
            return false;
        }
        static_cast<void>(process.waitForFinished(wait));
        if(process.state() != QProcess::NotRunning)
        {
            process_group_id =
                resolve_process_group(process, process_group_id);
            kill_process_group(process_group_id);
            process.kill();
        }
    }
    kill_process_group(process_group_id);
    return true;
}

auto finish_background_reap(
    QProcess& process,
    const qint64 known_process_group_id) -> void
{
    auto process_group_id =
        resolve_process_group(process, known_process_group_id);
    while(process.state() != QProcess::NotRunning)
    {
        process_group_id =
            resolve_process_group(process, process_group_id);
        kill_process_group(process_group_id);
        process.kill();
        static_cast<void>(process.waitForFinished(
            static_cast<int>(wait_slice.count())));
    }
    kill_process_group(process_group_id);
}

enum class process_stream : std::uint8_t
{
    standard_output,
    standard_error,
};

[[nodiscard]] auto drain_channel(
    QProcess& process,
    const process_stream stream,
    const std::size_t limit,
    std::size_t& consumed,
    std::string* retained_output)
    -> std::optional<github_cli_process_failure>
{
    const auto channel =
        stream == process_stream::standard_output
        ? QProcess::StandardOutput
        : QProcess::StandardError;
    const auto failure =
        stream == process_stream::standard_output
        ? github_cli_process_failure::standard_output_limit
        : github_cli_process_failure::standard_error_limit;
    process.setReadChannel(channel);

    std::array<char, 16U * 1024U> buffer{};
    while(process.bytesAvailable() > 0)
    {
        const auto remaining = limit - consumed;
        const auto requested = std::min<std::size_t>(
            buffer.size(),
            remaining + 1U);
        const auto bytes_read = process.read(
            buffer.data(),
            static_cast<qint64>(requested));
        if(bytes_read < 0)
        {
            return failure;
        }
        if(bytes_read == 0)
        {
            break;
        }

        const auto byte_count =
            static_cast<std::size_t>(bytes_read);
        if(byte_count > remaining)
        {
            return failure;
        }
        if(retained_output != nullptr)
        {
            retained_output->append(buffer.data(), byte_count);
        }
        consumed += byte_count;
    }
    return std::nullopt;
}

struct process_execution
{
    github_cli_process_result Result;
    std::unique_ptr<QProcess> AwaitingReap;
    qint64 ProcessGroupId = 0;
};

[[nodiscard]] auto failed_execution(
    std::unique_ptr<QProcess>& process,
    const qint64 known_process_group_id,
    const std::chrono::milliseconds reap_timeout,
    const github_cli_process_failure failure) -> process_execution
{
    const auto process_group_id =
        resolve_process_group(*process, known_process_group_id);
    if(
        kill_process_group_and_reap(
            *process,
            process_group_id,
            reap_timeout))
    {
        return {
            .Result = std::unexpected{failure},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    return {
        .Result = std::unexpected{
            github_cli_process_failure::reap_failure},
        .AwaitingReap = std::move(process),
        .ProcessGroupId = process_group_id,
    };
}

[[nodiscard]] auto execute_process(
    const github_cli_process_request& request,
    const std::stop_token stop,
    const std::atomic_bool& shutdown_requested,
    const clock::time_point deadline) -> process_execution
{
    validated_process_request validated;
    try
    {
        if(!validate_request(request, validated))
        {
            return {
                .Result = std::unexpected{
                    github_cli_process_failure::invalid_request},
                .AwaitingReap = nullptr,
                .ProcessGroupId = 0,
            };
        }
    }
    catch(...)
    {
        return {
            .Result = std::unexpected{
                github_cli_process_failure::invalid_request},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    if(
        shutdown_requested.load(std::memory_order_acquire)
        || stop.stop_requested())
    {
        return {
            .Result = std::unexpected{
                github_cli_process_failure::cancelled},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    if(clock::now() >= deadline)
    {
        return {
            .Result = std::unexpected{
                github_cli_process_failure::timeout},
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }

    auto process = std::make_unique<QProcess>();
    const auto process_group_id =
        std::make_shared<qint64>(0);
    try
    {
        process->setProgram(validated.Executable);
        process->setArguments(validated.Arguments);
        process->setProcessEnvironment(validated.Environment);
        process->setWorkingDirectory(validated.WorkingDirectory);
        process->setProcessChannelMode(QProcess::SeparateChannels);
        process->setInputChannelMode(QProcess::ManagedInputChannel);
#if defined(Q_OS_UNIX)
        process->setUnixProcessParameters(
            QProcess::UnixProcessFlag::CloseFileDescriptors
            | QProcess::UnixProcessFlag::DisableCoreDumps
            | QProcess::UnixProcessFlag::ResetSignalHandlers
            | QProcess::UnixProcessFlag::CreateNewSession);
#endif

        std::string standard_output;
        standard_output.reserve(std::min<std::size_t>(
            request.StandardOutputLimit,
            64U * 1024U));
        std::size_t standard_output_bytes = 0;
        std::size_t standard_error_bytes = 0;

        const auto discard_output = [&standard_output]() noexcept
        {
            std::fill(
                standard_output.begin(),
                standard_output.end(),
                '\0');
            standard_output.clear();
        };
        const auto failed =
            [&](const github_cli_process_failure failure)
            -> process_execution
        {
            discard_output();
            return failed_execution(
                process,
                *process_group_id,
                request.ReapTimeout,
                failure);
        };
        const auto drain_streams =
            [&]() -> std::optional<github_cli_process_failure>
        {
            if(
                const auto failure = drain_channel(
                    *process,
                    process_stream::standard_output,
                    request.StandardOutputLimit,
                    standard_output_bytes,
                    &standard_output);
                failure.has_value())
            {
                return failure;
            }
            if(
                const auto failure = drain_channel(
                    *process,
                    process_stream::standard_error,
                    request.StandardErrorLimit,
                    standard_error_bytes,
                    nullptr);
                failure.has_value())
            {
                return failure;
            }
            process->setReadChannel(QProcess::StandardOutput);
            return std::nullopt;
        };
        const auto terminal_failure =
            [&]() -> std::optional<github_cli_process_failure>
        {
            if(
                shutdown_requested.load(std::memory_order_acquire)
                || stop.stop_requested())
            {
                return github_cli_process_failure::cancelled;
            }
            if(clock::now() >= deadline)
            {
                return github_cli_process_failure::timeout;
            }
            return std::nullopt;
        };

        QObject::connect(
            process.get(),
            &QProcess::started,
            process.get(),
            [
                process_pointer = process.get(),
                process_group_id
            ]() noexcept
            {
                *process_group_id =
                    process_pointer->processId();
            },
            Qt::DirectConnection);
        QObject::connect(
            process.get(),
            &QProcess::finished,
            process.get(),
            [process_group_id](
                const int,
                const QProcess::ExitStatus) noexcept
            {
                kill_process_group(*process_group_id);
            },
            Qt::DirectConnection);
        if(
            shutdown_requested.load(std::memory_order_acquire)
            || stop.stop_requested())
        {
            return failed(
                github_cli_process_failure::cancelled);
        }
        if(clock::now() >= deadline)
        {
            return failed(github_cli_process_failure::timeout);
        }
        process->start();
        *process_group_id = resolve_process_group(
            *process,
            *process_group_id);
        for(;;)
        {
            *process_group_id = resolve_process_group(
                *process,
                *process_group_id);
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }

            if(process->waitForStarted(wait_milliseconds(deadline)))
            {
                if(*process_group_id == 0)
                {
                    *process_group_id =
                        process->processId();
                }
                break;
            }
            if(process->state() == QProcess::NotRunning)
            {
                return failed(
                    github_cli_process_failure::start_failure);
            }
        }

        std::size_t input_offset = 0;
        while(input_offset < request.StandardInput.size())
        {
            const auto remaining =
                request.StandardInput.size() - input_offset;
            const auto accepted = process->write(
                request.StandardInput.data() + input_offset,
                static_cast<qint64>(remaining));
            if(accepted < 0)
            {
                return failed(
                    github_cli_process_failure::write_failure);
            }
            input_offset += static_cast<std::size_t>(accepted);

            if(const auto failure = drain_streams(); failure.has_value())
            {
                return failed(*failure);
            }
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }
            if(
                accepted == 0
                && !process->waitForBytesWritten(
                    wait_milliseconds(deadline))
                && process->state() == QProcess::NotRunning)
            {
                return failed(
                    github_cli_process_failure::write_failure);
            }
        }

        while(process->bytesToWrite() > 0)
        {
            static_cast<void>(
                process->waitForBytesWritten(
                    wait_milliseconds(deadline)));
            if(const auto failure = drain_streams(); failure.has_value())
            {
                return failed(*failure);
            }
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }
            if(process->state() == QProcess::NotRunning)
            {
                return failed(
                    github_cli_process_failure::write_failure);
            }
        }
        process->closeWriteChannel();

        while(process->state() != QProcess::NotRunning)
        {
            process->setReadChannel(QProcess::StandardOutput);
            static_cast<void>(
                process->waitForReadyRead(
                    wait_milliseconds(deadline)));
            if(const auto failure = drain_streams(); failure.has_value())
            {
                return failed(*failure);
            }
            if(const auto failure = terminal_failure(); failure.has_value())
            {
                return failed(*failure);
            }
        }

        if(const auto failure = drain_streams(); failure.has_value())
        {
            return failed(*failure);
        }
        if(const auto failure = terminal_failure(); failure.has_value())
        {
            return failed(*failure);
        }
        if(process->exitStatus() == QProcess::CrashExit)
        {
            return failed(
                github_cli_process_failure::process_crash);
        }
        if(process->exitCode() != 0)
        {
            return failed(
                github_cli_process_failure::nonzero_exit);
        }
        if(standard_error_bytes != 0)
        {
            return failed(
                github_cli_process_failure::unexpected_standard_error);
        }

        kill_process_group(*process_group_id);
        return {
            .Result = github_cli_process_success{
                .StandardOutput = std::move(standard_output),
            },
            .AwaitingReap = nullptr,
            .ProcessGroupId = 0,
        };
    }
    catch(...)
    {
        return failed_execution(
            process,
            *process_group_id,
            request.ReapTimeout,
            github_cli_process_failure::start_failure);
    }
}

struct process_work_item
{
    github_cli_process_request Request;
    std::stop_token Stop;
    clock::time_point Deadline;
    std::mutex Mutex;
    std::condition_variable Completed;
    std::optional<github_cli_process_result> Result;
};

class process_worker final
{
public:
    explicit process_worker(
        std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
        : TestSeam{std::move(test_seam)},
          Worker{[this]
        {
            worker_loop();
        }}
    {
    }

    process_worker(const process_worker&) = delete;
    auto operator=(const process_worker&) -> process_worker& = delete;
    ~process_worker()
    {
        ShutdownRequested.store(true, std::memory_order_release);
        WorkAvailable.notify_all();
        SlotAvailable.notify_all();
        if(Worker.joinable())
        {
            Worker.join();
        }
    }

    [[nodiscard]] auto submit(
        const github_cli_process_request& request,
        const std::stop_token stop) -> github_cli_process_result
    {
        if(!valid_request_shape(request))
        {
            return std::unexpected{
                github_cli_process_failure::invalid_request};
        }
        if(stop.stop_requested())
        {
            return std::unexpected{
                github_cli_process_failure::cancelled};
        }

        std::shared_ptr<process_work_item> item;
        try
        {
            item = std::make_shared<process_work_item>();
            item->Request = request;
            item->Stop = stop;
            item->Deadline = clock::now() + request.Timeout;
        }
        catch(...)
        {
            return std::unexpected{
                github_cli_process_failure::start_failure};
        }

        {
            std::unique_lock lock{Mutex};
            if(ShutdownRequested.load(std::memory_order_acquire))
            {
                return std::unexpected{
                    github_cli_process_failure::cancelled};
            }
            while(Busy)
            {
                if(ShutdownRequested.load(std::memory_order_acquire))
                {
                    return std::unexpected{
                        github_cli_process_failure::cancelled};
                }
                if(Quarantined)
                {
                    return std::unexpected{
                        github_cli_process_failure::reap_failure};
                }
                if(stop.stop_requested())
                {
                    return std::unexpected{
                        github_cli_process_failure::cancelled};
                }
                if(clock::now() >= item->Deadline)
                {
                    return std::unexpected{
                        github_cli_process_failure::timeout};
                }
                SlotAvailable.wait_for(lock, wait_slice);
            }
            Busy = true;
            Pending = item;
        }
        WorkAvailable.notify_one();

        std::unique_lock result_lock{item->Mutex};
        const auto completion_deadline =
            item->Deadline + request.ReapTimeout + wait_slice;
        if(
            !item->Completed.wait_until(
                result_lock,
                completion_deadline,
                [&]
                {
                    return item->Result.has_value();
                }))
        {
            {
                std::lock_guard lock{Mutex};
                if(Busy)
                {
                    Quarantined = true;
                }
            }
            SlotAvailable.notify_all();
            return std::unexpected{
                github_cli_process_failure::reap_failure};
        }
        return std::move(*item->Result);
    }

private:
    auto publish(
        const std::shared_ptr<process_work_item>& item,
        github_cli_process_result result) -> void
    {
        {
            std::lock_guard lock{item->Mutex};
            item->Result.emplace(std::move(result));
        }
        item->Completed.notify_one();
    }

    auto worker_loop() -> void
    {
        for(;;)
        {
            std::shared_ptr<process_work_item> item;
            {
                std::unique_lock lock{Mutex};
                WorkAvailable.wait(
                    lock,
                    [&]
                    {
                        return Pending != nullptr
                            || ShutdownRequested.load(
                                std::memory_order_acquire);
                    });
                if(Pending == nullptr)
                {
                    return;
                }
                item = std::move(Pending);
            }

            std::optional<process_execution> execution;
            try
            {
                if(TestSeam != nullptr)
                {
                    TestSeam->before_execute();
                }
                execution.emplace(execute_process(
                item->Request,
                item->Stop,
                ShutdownRequested,
                item->Deadline));
            }
            catch(...)
            {
                publish(
                    item,
                    std::unexpected{
                        github_cli_process_failure::start_failure});
                {
                    std::lock_guard lock{Mutex};
                    Quarantined = false;
                    Busy = false;
                }
                SlotAvailable.notify_all();
                continue;
            }

            if(execution->AwaitingReap != nullptr)
            {
                {
                    std::lock_guard lock{Mutex};
                    Quarantined = true;
                }
                SlotAvailable.notify_all();
            }
            publish(item, std::move(execution->Result));

            if(execution->AwaitingReap != nullptr)
            {
                finish_background_reap(
                    *execution->AwaitingReap,
                    execution->ProcessGroupId);
                execution->AwaitingReap.reset();
            }
            {
                std::lock_guard lock{Mutex};
                Quarantined = false;
                Busy = false;
            }
            SlotAvailable.notify_all();
        }
    }

    std::mutex Mutex;
    std::condition_variable WorkAvailable;
    std::condition_variable SlotAvailable;
    std::shared_ptr<process_work_item> Pending;
    bool Busy = false;
    bool Quarantined = false;
    std::atomic_bool ShutdownRequested = false;
    std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
    std::thread Worker;
};

class qprocess_github_cli_process_runner final
    : public github_cli_process_port
{
public:
    explicit qprocess_github_cli_process_runner(
        std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
        : TestSeam{std::move(test_seam)}
    {
    }

    [[nodiscard]] auto run(
        const github_cli_process_request& request,
        const std::stop_token stop) -> github_cli_process_result override
    {
        try
        {
            process_worker* worker = nullptr;
            {
                std::lock_guard lock{WorkerMutex};
                if(Worker == nullptr)
                {
                    Worker = std::make_unique<process_worker>(
                        TestSeam);
                }
                worker = Worker.get();
            }
            return worker->submit(request, stop);
        }
        catch(...)
        {
            return std::unexpected{
                github_cli_process_failure::start_failure};
        }
    }

private:
    std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
    std::mutex WorkerMutex;
    std::unique_ptr<process_worker> Worker;
};

[[nodiscard]] auto make_qprocess_github_cli_process_runner(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
    -> std::unique_ptr<github_cli_process_port>
{
    return std::make_unique<qprocess_github_cli_process_runner>(
        std::move(test_seam));
}
}

auto make_github_cli_process_runner()
    -> std::unique_ptr<github_cli_process_port>
{
    return make_qprocess_github_cli_process_runner(nullptr);
}

auto make_github_cli_process_runner_for_test(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
    -> std::unique_ptr<github_cli_process_port>
{
    return make_qprocess_github_cli_process_runner(
        std::move(test_seam));
}
}
