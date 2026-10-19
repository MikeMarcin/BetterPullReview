#!/usr/bin/env python3
from __future__ import annotations

import sys
import re
from pathlib import Path
import shutil
import subprocess
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
VERIFY_SCRIPT = REPOSITORY_ROOT / "cmake" / "VerifyArchitectureBoundaries.cmake"
DESIGNATED_LIVE_TEST_NETWORK_RUNNER = (
    REPOSITORY_ROOT
    / "tests"
    / "network_isolation"
    / "designated_live_mutation_runner.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_CANARY = (
    REPOSITORY_ROOT
    / "tests"
    / "network_isolation"
    / "designated_live_mutation_canary.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_CANARY_HEADER = (
    REPOSITORY_ROOT
    / "tests"
    / "network_isolation"
    / "designated_live_mutation_canary.hpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_CHILD = (
    REPOSITORY_ROOT
    / "tests"
    / "network_isolation"
    / "designated_live_mutation_child.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_CHILD_HEADER = (
    REPOSITORY_ROOT
    / "tests"
    / "network_isolation"
    / "designated_live_mutation_child.hpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_CMAKE = (
    REPOSITORY_ROOT / "tests" / "CMakeLists.txt"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_PUBLIC_OBSERVER_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "github"
    / "src"
    / "designated_live_public_observer.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_PUBLIC_OBSERVER_INTERFACE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "github"
    / "src"
    / "designated_live_public_observer.hpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "app"
    / "src"
    / "designated_live_macos_supervisor.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "app"
    / "src"
    / "designated_live_macos_supervisor_identity.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "app"
    / "src"
    / "designated_live_macos_supervisor_launch.cpp"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT = (
    REPOSITORY_ROOT / "scripts" / "designated_live_macos_capsule.py"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_STAGE_ZERO_PRELOADER = (
    REPOSITORY_ROOT / "cmake" / "BprDesignatedLiveStageZero.cmake"
).read_text(encoding="utf-8")
DESIGNATED_LIVE_TEST_ISOLATION = (
    REPOSITORY_ROOT / "cmake" / "BprTestIsolation.cmake"
).read_text(encoding="utf-8")

DESIGNATED_LIVE_MACOS_SUPERVISOR_TARGET = """\
target_sources(
    tool_better_pull_review_designated_live_supervisor
    PRIVATE
        src/designated_live_macos_supervisor.cpp
        src/designated_live_macos_supervisor_launch.cpp
)
file(
    SHA256
    "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
    BPR_DESIGNATED_LIVE_VERIFIER_SHA256
)
target_compile_definitions(
    tool_better_pull_review_designated_live_supervisor
    PRIVATE
        "BPR_DESIGNATED_LIVE_VERIFIER_SHA256=\"${BPR_DESIGNATED_LIVE_VERIFIER_SHA256}\""
)
add_custom_command(
    OUTPUT capsule.stamp
    COMMAND ${bpr_designated_live_probe_command}
)
set(bpr_designated_live_capsule_image capsule.dmg)
set(bpr_designated_live_source_manifest source-manifest.txt)
bpr_designated_live_stage_zero_command(
    bpr_designated_live_probe_command
)
"""

ADAPTER_ROOT = """\
add_subdirectory(github)
add_subdirectory(git)
add_subdirectory(agent)
"""

DEPENDENCIES_TARGET = """\
function(bpr_configure_dependencies)
bpr_acquire_quill()
bpr_require_dependency_target(quill::quill Quill)
bpr_include_builtin_find_module(Threads)
target_link_libraries(example PRIVATE Qt6::Core)
target_link_libraries(example PRIVATE LibSecret::LibSecret)
target_link_libraries(example PRIVATE Python3::Interpreter)
target_link_libraries(example PRIVATE Git::Git)
endfunction()
"""

GIT_TARGET = (
    REPOSITORY_ROOT / "src" / "adapters" / "git" / "CMakeLists.txt"
).read_text(encoding="utf-8")
GIT_PROCESS_RUNNER_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "git"
    / "src"
    / "installed_git_process.cpp"
).read_text(encoding="utf-8")
# [[BPR-1436]] The shared QProcess supervision, as the repository declares it.
PROCESS_SUPPORT_CMAKE_RELATIVE = "src/adapters/process_support/CMakeLists.txt"
PROCESS_SUPPORT_SOURCE_RELATIVE = (
    "src/adapters/process_support/src/process_supervision.cpp"
)
PROCESS_SUPPORT_HEADER_RELATIVE = (
    "src/adapters/process_support/include/bpr/adapters/process_support/"
    "process_supervision.hpp"
)
PROCESS_SUPPORT_CMAKE = (REPOSITORY_ROOT / PROCESS_SUPPORT_CMAKE_RELATIVE).read_text(
    encoding="utf-8"
)
PROCESS_SUPPORT_SOURCE = (
    REPOSITORY_ROOT / PROCESS_SUPPORT_SOURCE_RELATIVE
).read_text(encoding="utf-8")
PROCESS_SUPPORT_HEADER = (
    REPOSITORY_ROOT / PROCESS_SUPPORT_HEADER_RELATIVE
).read_text(encoding="utf-8")
GIT_PROCESS_PORT = (
    REPOSITORY_ROOT
    / "src"
    / "application"
    / "include"
    / "bpr"
    / "application"
    / "git_process_port.hpp"
).read_text(encoding="utf-8")
GIT_PROCESS_FACTORY = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "git"
    / "include"
    / "bpr"
    / "adapters"
    / "git"
    / "installed_git_process.hpp"
).read_text(encoding="utf-8")

# [[BPR-0825]] The header-only platform target, as the repository declares it.
PLATFORM_HEADER = "src/platform/include/bpr/platform/windows_private_object.hpp"
PLATFORM_TARGET = (REPOSITORY_ROOT / "src/platform/CMakeLists.txt").read_text(
    encoding="utf-8"
)
# [[BPR-1115]] The Windows launch restriction the QProcess launchers share.
PLATFORM_STANDARD_HANDLES_HEADER = (
    "src/platform/include/bpr/platform/windows_standard_handles.hpp"
)
PLATFORM_STANDARD_HANDLES_SOURCE = (
    REPOSITORY_ROOT / PLATFORM_STANDARD_HANDLES_HEADER
).read_text(encoding="utf-8")
AGENT_TARGET = (
    REPOSITORY_ROOT / "src" / "adapters" / "agent" / "CMakeLists.txt"
).read_text(encoding="utf-8")
AGENT_PROCESS_RUNNER_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "agent"
    / "src"
    / "agent_process_launcher.cpp"
).read_text(encoding="utf-8")
AGENT_PROTOCOL_LOOP_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "agent"
    / "src"
    / "agent_protocol_loop.cpp"
).read_text(encoding="utf-8")
AGENT_WINDOWS_PROCESS_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "agent"
    / "src"
    / "windows_agent_process.cpp"
).read_text(encoding="utf-8")
# [[BPR-0622]] made the Windows supervisor a selected agent process source the
# gate requires; a fixture tree without it stops every case at that refusal.
AGENT_WINDOWS_SUPERVISOR_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "adapters"
    / "agent"
    / "src"
    / "windows_agent_supervisor.cpp"
).read_text(encoding="utf-8")

APPLE_KEYCHAIN_TARGET = """\
if(APPLE)
    find_library(BPR_TEST_SECURITY_FRAMEWORK Security REQUIRED)
    find_library(
        BPR_TEST_CORE_FOUNDATION_FRAMEWORK
        CoreFoundation
        REQUIRED
    )
    target_sources(
        better_pull_review_adapter_github
        INTERFACE
            src/macos_keychain.cpp
    )
    target_link_libraries(
        better_pull_review_adapter_github
        INTERFACE
            ${BPR_TEST_SECURITY_FRAMEWORK}
            ${BPR_TEST_CORE_FOUNDATION_FRAMEWORK}
    )
endif()
"""

WINDOWS_CREDENTIAL_TARGET = """\
if(WIN32)
    target_sources(
        better_pull_review_adapter_github
        INTERFACE
            src/windows_credential_manager.cpp
    )
    target_link_libraries(
        better_pull_review_adapter_github
        INTERFACE
            Advapi32
    )
endif()
"""

LINUX_SECRET_SERVICE_TARGET = """\
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(
        better_pull_review_adapter_github
        INTERFACE
            src/linux_secret_service.cpp
    )
    target_link_libraries(
        better_pull_review_adapter_github
        INTERFACE
            LibSecret::LibSecret
    )
endif()
"""

GITHUB_TARGET = """\
add_library(better_pull_review_adapter_github INTERFACE)
add_library(BetterPullReview::AdapterGitHub ALIAS better_pull_review_adapter_github)
target_link_libraries(
    better_pull_review_adapter_github
    INTERFACE
        BetterPullReview::Application
    PRIVATE
        Threads::Threads
)
target_link_libraries(
    better_pull_review_adapter_github
    PRIVATE
        Qt6::Network
)
target_sources(
    better_pull_review_adapter_github
    INTERFACE
        src/github_cli_process_runner.cpp
        src/gh_cli_transport.cpp
        src/github_wire_request_validation.cpp
        src/native_https_transport.cpp
        src/platform_body_digest.cpp
)
if(BUILD_TESTING)
    target_compile_definitions(
        better_pull_review_adapter_github
        PRIVATE
            BPR_NATIVE_HTTPS_TESTING=1
    )
endif()
""" + (
    APPLE_KEYCHAIN_TARGET
    + WINDOWS_CREDENTIAL_TARGET
    + LINUX_SECRET_SERVICE_TARGET
)

DESIGNATED_LIVE_PUBLIC_OBSERVER_TARGET = """\
if(APPLE AND (BUILD_TESTING OR BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE))
    add_library(
        better_pull_review_designated_live_public_observer
        STATIC
            src/designated_live_public_observer.cpp
    )
    if(BUILD_TESTING)
        target_compile_definitions(
            better_pull_review_designated_live_public_observer
            PRIVATE
                BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING=1
        )
    endif()
endif()
"""

GITHUB_CLI_PROCESS_INTERFACE = """\
#pragma once
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct github_cli_process_request {
    std::string Program;
    std::vector<std::string> Arguments;
    std::vector<std::string> Environment;
    std::string WorkingDirectory;
    std::vector<std::byte> StandardInput;
    std::size_t StandardInputLimit;
    std::size_t StandardOutputLimit;
    std::size_t StandardErrorLimit;
    std::chrono::milliseconds Timeout;
    std::chrono::milliseconds ReapTimeout;
};

class github_cli_process_port {
public:
    virtual ~github_cli_process_port() = default;
};
"""

GITHUB_CLI_PROCESS_RUNNER_INTERFACE = """\
#pragma once
#include "github_cli_process.hpp"
#include <memory>

class github_cli_process_runner_test_seam {
public:
    virtual ~github_cli_process_runner_test_seam() = default;
    virtual auto before_execute() -> void = 0;
};

auto make_github_cli_process_runner()
    -> std::unique_ptr<github_cli_process_port>;
auto make_github_cli_git_acquisition_process_runner(
    github_cli_process_file_limits aFileLimits)
    -> std::unique_ptr<github_cli_process_port>;
auto make_github_cli_process_runner_for_test(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
    -> std::unique_ptr<github_cli_process_port>;
"""

GITHUB_CLI_PROCESS_RUNNER_SOURCE = """\
#include "github_cli_process_runner.hpp"
#include "bpr/adapters/process_support/process_supervision.hpp"
#include <QProcess>
#include <QProcessEnvironment>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#if defined(Q_OS_WIN)
#include "bpr/platform/windows_standard_handles.hpp"
#endif

using clock = std::chrono::steady_clock;

[[nodiscard]] auto contains_nul(
    const std::string_view value) noexcept -> bool {
    return value.find('\\0') != std::string_view::npos;
}

[[nodiscard]] auto valid_environment_name(
    const std::string_view name) noexcept -> bool {
    return !name.empty();
}

[[nodiscard]] auto equal_environment_names(
    const std::string_view left,
    const std::string_view right) noexcept -> bool {
    return left == right;
}

[[nodiscard]] auto add_bounded(
    std::size_t& total,
    std::size_t amount,
    std::size_t limit) noexcept -> bool {
    if(total > limit || amount > limit - total) {
        return false;
    }
    total += amount;
    return true;
}

[[nodiscard]] auto valid_request_shape(
    const github_cli_process_request& request) noexcept -> bool {
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
                github_cli_process_max_reap_timeout)) {
        return false;
    }

    std::size_t argument_bytes = 0;
    for(const auto& argument : request.Arguments) {
        if(
            argument.empty()
            || argument.size() > github_cli_process_max_argument_bytes
            || contains_nul(argument)
            || !add_bounded(
                argument_bytes,
                argument.size(),
                github_cli_process_max_arguments_bytes)) {
            return false;
        }
    }

    std::size_t environment_bytes = 0;
#if defined(_WIN32)
    bool has_empty_path = false;
    bool has_system_root = false;
#endif
    for(std::size_t index = 0; index < request.Environment.size(); ++index) {
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
                github_cli_process_max_environment_bytes)) {
            return false;
        }
#if defined(_WIN32)
        if(equal_environment_names(entry.Name, "PATH")) {
            has_empty_path = entry.Value.empty();
        }
        if(equal_environment_names(entry.Name, "SystemRoot")) {
            has_system_root = !entry.Value.empty();
        }
#endif
        for(std::size_t prior = 0; prior < index; ++prior) {
            if(equal_environment_names(
                   entry.Name,
                   request.Environment[prior].Name)) {
                return false;
            }
        }
    }
#if defined(_WIN32)
    if(!has_empty_path || !has_system_root) {
        return false;
    }
#endif
    return true;
}

[[nodiscard]] auto decode_utf8(
    const std::string_view,
    QString&) -> bool {
    return true;
}

[[nodiscard]] auto exact_canonical_file() -> bool {
    return true;
}

bool validate_request(
    const github_cli_process_request& request,
    validated_process_request& validated) {
    const char* neutral_repository_marker = ".git";
#if defined(_WIN32)
    bool has_empty_path = false;
    bool has_system_root = false;
#endif
    for(const auto& entry : request.Environment) {
#if defined(_WIN32)
        if(equal_environment_names(entry.Name, "PATH")) {
            has_empty_path = entry.Value.empty();
        }
        if(equal_environment_names(entry.Name, "SystemRoot")) {
            has_system_root = !entry.Value.empty();
        }
#endif
        child_environment.insert(entry.Name, entry.Value);
    }
#if defined(_WIN32)
    if(!has_empty_path || !has_system_root) {
        return false;
    }
#endif
    validated.Environment = child_environment;
    return true;
}

std::optional<github_cli_process_failure> drain_channel(
    QProcess& process,
    QProcess::ProcessChannel channel,
    std::size_t limit,
    std::size_t& consumed) {
    process.setReadChannel(channel);
    std::array<char, 16U * 1024U> buffer{};
    while(process.bytesAvailable() > 0) {
        const auto remaining = limit - consumed;
        const auto requested = std::min<std::size_t>(
            buffer.size(),
            remaining + 1U);
        const auto bytes_read = process.read(
            buffer.data(),
            static_cast<qint64>(requested));
        const auto byte_count = static_cast<std::size_t>(bytes_read);
        if(byte_count > remaining) {
            return github_cli_process_failure::standard_output_limit;
        }
        consumed += byte_count;
    }
    return std::nullopt;
}

struct process_execution {
    github_cli_process_result Result;
    std::unique_ptr<QProcess> AwaitingReap;
    qint64 ProcessGroupId = 0;
};

process_execution failed_execution(
    std::unique_ptr<QProcess>& process,
    const qint64 known_process_group_id,
    std::chrono::milliseconds reap_timeout,
    github_cli_process_failure failure) {
    const auto process_group_id =
        resolve_process_group(*process, known_process_group_id);
    if(
        kill_process_group_and_reap(
            *process,
            process_group_id,
            reap_timeout)) {
        return {.Result = std::unexpected{failure}};
    }
    return {
        .Result = std::unexpected{github_cli_process_failure::reap_failure},
        .AwaitingReap = std::move(process),
        .ProcessGroupId = process_group_id,
    };
}

process_execution execute_process(
    const github_cli_process_request& request,
    std::stop_token stop,
    const std::atomic_bool& shutdown_requested,
    clock::time_point deadline) {
    validated_process_request validated;
    validate_request(request, validated);
    if(request.StandardInput.size() > request.StandardInputLimit
       || request.StandardOutputLimit == 0
       || request.StandardErrorLimit == 0) {
        return {.Result = std::unexpected{
            github_cli_process_failure::invalid_request}};
    }

    auto process = std::make_unique<QProcess>();
    const auto process_group_id =
        std::make_shared<qint64>(0);
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
#elif defined(Q_OS_WIN)
    bpr::platform::windows_standard_handles::launch_restriction inheritance;
    process->setCreateProcessArgumentsModifier([&inheritance, aFailStandardHandleList](QProcess::CreateProcessArguments* const aArguments) {
        if (aFailStandardHandleList) {
            aArguments->startupInfo->hStdError = nullptr;
        }
        inheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles);
    });
#endif
    QObject::connect(
        process.get(),
        &QProcess::started,
        process.get(),
        [
            process_pointer = process.get(),
            process_group_id
        ]() noexcept {
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
            const QProcess::ExitStatus) noexcept {
            kill_process_group(*process_group_id);
        },
        Qt::DirectConnection);
    const auto failed = [&](github_cli_process_failure failure) {
        return failed_execution(
            process,
            *process_group_id,
            request.ReapTimeout,
            failure);
    };
    const auto terminal_failure = [&]()
        -> std::optional<github_cli_process_failure> {
        if(stop.stop_requested()) {
            return github_cli_process_failure::cancelled;
        }
        return std::nullopt;
    };
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
    for(;;) {
        *process_group_id = resolve_process_group(
            *process,
            *process_group_id);
        if(const auto failure = terminal_failure(); failure.has_value()) {
            return failed(*failure);
        }
        if(process->waitForStarted(request.Timeout.count())) {
            break;
        }
        if(process->state() == QProcess::NotRunning) {
            return failed(github_cli_process_failure::start_failure);
        }
    }
#if defined(Q_OS_WIN)
    if (!inheritance.restricted()) {
        return failed(github_cli_process_failure::start_failure);
    }
#endif
    process->write(
        request.StandardInput.data(),
        static_cast<qint64>(request.StandardInput.size()));
    process->closeWriteChannel();
    process->setReadChannel(QProcess::StandardOutput);
    process->waitForReadyRead(request.Timeout.count());
    std::size_t consumed = 0;
    drain_channel(
        *process,
        QProcess::StandardOutput,
        request.StandardOutputLimit,
        consumed);
    if(process->state() != QProcess::NotRunning) {
        return failed(github_cli_process_failure::timeout);
    }
    kill_process_group(*process_group_id);
    return {
        .Result = github_cli_process_success{},
    };
}

struct process_work_item {
    github_cli_process_request Request;
    std::stop_token Stop;
    clock::time_point Deadline;
    std::mutex Mutex;
    std::condition_variable Completed;
    std::optional<github_cli_process_result> Result;
};

struct process_lane_traits {
    static github_cli_process_result failure(
        process_support::lane_failure lane_failure) {
        switch(lane_failure) {
        case process_support::lane_failure::reap_failure:
            return std::unexpected{
                github_cli_process_failure::reap_failure};
        default:
            return std::unexpected{
                github_cli_process_failure::start_failure};
        }
    }

    process_execution execute(
        process_work_item& item,
        const std::atomic_bool& shutdown_requested) {
        if(TestSeam != nullptr) {
            TestSeam->before_execute();
        }
        return execute_process(
            item.Request,
            item.Stop,
            shutdown_requested,
            item.Deadline);
    }

    bool finish_reap(process_execution& aExecution) const {
        return process_support::finish_background_reap(
            *aExecution.AwaitingReap,
            aExecution.ProcessGroupId,
            github_cli_process_max_reap_timeout,
            tree_kill_for(TestSeam));
    }

    std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
};

class process_worker final {
public:
    explicit process_worker(
        std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
        : Lane{process_lane_traits{.TestSeam = std::move(test_seam)}} {}

    github_cli_process_result submit(
        const github_cli_process_request& request,
        std::stop_token stop) {
        if(!valid_request_shape(request)) {
            return std::unexpected{
                github_cli_process_failure::invalid_request};
        }
        if(stop.stop_requested()) {
            return std::unexpected{
                github_cli_process_failure::cancelled};
        }
        std::shared_ptr<process_work_item> item;
        try {
            item = std::make_shared<process_work_item>();
            item->Request = request;
            item->Stop = stop;
            item->Deadline = clock::now() + request.Timeout;
        }
        catch(...) {
            return std::unexpected{
                github_cli_process_failure::start_failure};
        }
        return Lane.submit(item, request.ReapTimeout);
    }

private:
    process_support::serialized_process_lane<process_lane_traits> Lane;
};

class qprocess_github_cli_process_runner final {
public:
    explicit qprocess_github_cli_process_runner(
        std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
        : TestSeam{std::move(test_seam)} {}

    github_cli_process_result run(
        const github_cli_process_request& request,
        std::stop_token stop) {
        process_worker* worker = nullptr;
        {
            std::lock_guard lock{WorkerMutex};
            if(Worker == nullptr) {
                Worker = std::make_unique<process_worker>(
                    TestSeam);
            }
            worker = Worker.get();
        }
        return worker->submit(request, stop);
    }

private:
    std::shared_ptr<github_cli_process_runner_test_seam> TestSeam;
    github_cli_process_file_limits FileLimits;
    std::mutex WorkerMutex;
    std::unique_ptr<process_worker> Worker;
};

std::unique_ptr<github_cli_process_port>
make_qprocess_github_cli_process_runner(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam) {
    return std::make_unique<qprocess_github_cli_process_runner>(
        std::move(test_seam));
}

auto make_github_cli_process_runner()
    -> std::unique_ptr<github_cli_process_port> {
    return make_qprocess_github_cli_process_runner(nullptr);
}

auto make_github_cli_git_acquisition_process_runner(
    github_cli_process_file_limits aFileLimits)
    -> std::unique_ptr<github_cli_process_port> {
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

auto
make_github_cli_process_runner_for_test(
    std::shared_ptr<github_cli_process_runner_test_seam> test_seam)
    -> std::unique_ptr<github_cli_process_port> {
    return make_qprocess_github_cli_process_runner(
        std::move(test_seam));
}
"""

GH_CLI_TRANSPORT_SOURCE = """\
#include "gh_cli_transport.hpp"
#include "github_wire_request_validation.hpp"

constexpr const char* neutral_repository_marker = ".git";
constexpr const char* arguments[] = {
    "api",
    "--hostname",
    "--include",
    "--method",
    "--input",
    "-",
};
constexpr const char* environment[] = {
    "GH_PROMPT_DISABLED=1",
    "GH_NO_UPDATE_NOTIFIER=1",
    "GH_NO_EXTENSION_UPDATE_NOTIFIER=1",
    "GH_SPINNER_DISABLED=1",
    "GH_TELEMETRY=0",
    "NO_COLOR=1",
    "CLICOLOR=0",
    "HOME",
    "XDG_RUNTIME_DIR",
    "DBUS_SESSION_BUS_ADDRESS",
    "USERPROFILE",
    "APPDATA",
    "LOCALAPPDATA",
    "SystemRoot",
    "PATH",
    "HTTPS_PROXY",
    "NO_PROXY",
    "SSL_CERT_FILE",
    "SSL_CERT_DIR",
};
"""

NATIVE_HTTPS_TRANSPORT_INTERFACE = """\
#pragma once
#include <chrono>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
class native_https_transport;
#if defined(BPR_NATIVE_HTTPS_TESTING)
struct native_https_test_configuration {
    std::string ProxyHost;
};
auto make_native_https_transport_for_test()
    -> std::unique_ptr<native_https_transport>;
#endif
"""

GITHUB_WIRE_VALIDATION_INTERFACE = """\
#pragma once
#include <expected>
auto validate_github_wire_request() -> std::expected<void, int>;
"""

GITHUB_WIRE_VALIDATION_SOURCE = """\
#include "github_wire_request_validation.hpp"
auto validate_github_wire_request() -> std::expected<void, int> {
    return validate_outbound_read_request();
}
"""

NATIVE_HTTPS_TRANSPORT_SOURCE = """\
#include "native_https_transport.hpp"
#include "github_wire_request_validation.hpp"
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslSocket>
#include <stop_token>

constexpr int maximum_system_proxy_candidates = 16;
struct active_hop_guard {};

void remaining_before_deadline() {}
bool proxy_candidate_retryable() { return true; }

void native_https_policy() {
    QNetworkAccessManager manager;
    QNetworkRequest request;
    QSslConfiguration tls;
    QNetworkProxyFactory::systemProxyForQuery();
    manager.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    tls.setPeerVerifyMode(QSslSocket::VerifyPeer);
    tls.setProtocol(QSsl::TlsV1_2OrLater);
    QSslConfiguration::systemCaCertificates();
    tls.setLocalCertificateChain({});
    tls.setBackendConfiguration({});
    auto signal = &QNetworkAccessManager::sslErrors;
    auto encrypted = &QNetworkReply::encrypted;
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("Accept-Language", "*");
    request.setTransferTimeout(1);
    manager.get(request)->setReadBufferSize(1);
    validate_github_redirect_target();
    validate_github_wire_request();
    QSslSocket::activeBackend();
}

void cancellation(std::stop_token stop) {
    std::stop_callback callback{stop, [] {}};
}

struct completion_state {
    struct done_state {
        void notify_one();
    } Done;
};

void complete(completion_state& state) {
    state.Done.notify_one();
}

#if defined(BPR_NATIVE_HTTPS_TESTING)
void fixed_loopback_fixture_policy();
#endif
"""

MACOS_KEYCHAIN_SOURCE = """\
#include <Security/Security.h>
#include <functional>
#include <map>
#include <mutex>

using callback = std::function<void()>;
std::map<int, callback> callbacks;
std::mutex callback_mutex;
namespace application { class effect_executor; }
application::effect_executor* selected_effect_lane = nullptr;

void selected_keychain_lifecycle() {
    SecKeychainRef keychain = nullptr;
    SecKeychainCopyDefault(&keychain);
    const void* query_contract[] = {
        kSecClassGenericPassword,
        kSecAttrService,
        kSecAttrAccount,
        kSecUseKeychain,
        kSecMatchSearchList,
        kSecUseAuthenticationUI,
        kSecUseAuthenticationUIFail,
        kSecReturnData,
        kSecMatchLimit,
        kSecMatchLimitOne,
    };
    SecItemAdd(query, nullptr);
    SecItemCopyMatching(query, nullptr);
    SecItemUpdate(query, attributes);
    SecItemDelete(query);
}
"""

WINDOWS_CREDENTIAL_MANAGER_SOURCE = """\
#include <wincred.h>

namespace application {
class effect_executor;
constexpr std::size_t maximum_github_credential_bytes = 2'560U;
}
application::effect_executor* selected_effect_lane = nullptr;

constexpr std::size_t windows_credential_manager_maximum_blob_bytes =
    5U * 512U;
static_assert(
    application::maximum_github_credential_bytes
    == windows_credential_manager_maximum_blob_bytes);

void selected_credential_manager_lifecycle() {
    CREDENTIALW credential{};
    PCREDENTIALW retrieved = nullptr;
    static_assert(667U <= CRED_MAX_GENERIC_TARGET_NAME_LENGTH);
    static_assert(
        2'560U == windows_credential_manager_maximum_blob_bytes);
    const DWORD blob_size = 512U;
    credential.Type = CRED_TYPE_GENERIC;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&credential, 0U);
    CredReadW(target, CRED_TYPE_GENERIC, 0U, &retrieved);
    const DWORD status = GetLastError();
    CredDeleteW(target, CRED_TYPE_GENERIC, 0U);
    if(
        credential.CredentialBlobSize
        <= windows_credential_manager_maximum_blob_bytes)
    {
        SecureZeroMemory(credential.CredentialBlob, blob_size);
    }
    CredFree(retrieved);
}
"""

LINUX_SECRET_SERVICE_SOURCE = """\
#include <libsecret/secret.h>

namespace application {
class effect_executor;
constexpr std::size_t maximum_github_credential_bytes = 2'560U;
constexpr std::size_t maximum_github_hostname_bytes = 253U;
}
application::effect_executor* selected_effect_lane = nullptr;

constexpr std::size_t linux_secret_service_authority_account_bytes = 574U;
static_assert(linux_secret_service_authority_account_bytes == 574U);
constexpr std::string_view canonical_service_bus_name =
    "org.freedesktop.secrets";
constexpr std::string_view production_schema_identifier =
    "org.betterpullreview.github-native-credential.secret-service.v2";
constexpr std::string_view integration_test_schema_identifier =
    "org.betterpullreview.github-native-credential."
    "secret-service.integration-test.v2";
constexpr std::string_view authority_attribute = "authority";
constexpr std::string_view schema_attribute = "xdg:schema";
constexpr std::string_view binary_content_type = "application/octet-stream";
constexpr std::string_view gnome_keyring_content_type = "text/plain";
constexpr gint prompt_dismiss_timeout_milliseconds = 1'000;

[[nodiscard]] consteval auto c_str(const std::string_view text)
    -> const char* {
    if(text.data()[text.size()] != '\\0') {
        throw "a string_view handed to a libsecret entry point must be "
              "null-terminated";
    }
    return text.data();
}

struct NonPromptingSecretService { SecretService ParentInstance; };
struct NonPromptingSecretServiceClass { SecretServiceClass ParentClass; };

void dismiss_prompt(SecretPrompt* prompt) {
    g_dbus_proxy_call_sync(
        G_DBUS_PROXY(prompt),
        "Dismiss",
        g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        prompt_dismiss_timeout_milliseconds,
        nullptr,
        nullptr);
}

void deny_prompt_sync(SecretPrompt* prompt) {
    dismiss_prompt(prompt);
}
void deny_prompt_async(SecretPrompt* prompt) {
    dismiss_prompt(prompt);
}
void deny_prompt_finish() {}
constexpr auto prompt_failure = SECRET_ERROR_IS_LOCKED;

G_DEFINE_TYPE(
    NonPromptingSecretService,
    non_prompting_secret_service,
    SECRET_TYPE_SERVICE)

void non_prompting_secret_service_class_init(
    NonPromptingSecretServiceClass* service_class) {
    auto* parent = SECRET_SERVICE_CLASS(service_class);
    parent->prompt_sync = deny_prompt_sync;
    parent->prompt_async = deny_prompt_async;
    parent->prompt_finish = deny_prompt_finish;
}

void selected_store_service_gate() {
    const auto service = service_proxy();
}
void selected_retrieve_service_gate() {
    const auto service = service_proxy();
}
void selected_delete_service_gate() {
    const auto service = service_proxy();
}
void selected_store_readiness_service_gate() {
    const auto service = service_proxy();
}

void selected_secret_service_lifecycle() {
    SecretSchema schema{};
    schema.flags = SECRET_SCHEMA_NONE;
    schema.attributes[0].name = authority_attribute;
    schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
    g_hash_table_insert(
        attributes,
        const_cast<char*>(c_str(authority_attribute)),
        const_cast<char*>(authority_account.data()));
    g_hash_table_lookup(attributes, c_str(authority_attribute));
    g_hash_table_lookup(attributes, c_str(schema_attribute));
    if(g_hash_table_size(attributes) != 2U) {}

    for(std::size_t nibble = 0U; nibble < 16U; ++nibble) {}
    append_hex_u64(authority.Profile.value());
    append_hex_u64(authority.ProfileRevision.value());
    append_hex_u64(authority.Host.value());
    append_hex_u64(authority.Account.value());
    append_hex_u8(
        output,
        offset,
        server_contract_discriminator(authority.ServerContract));
    const auto hostname = authority.Hostname.value();
    append_hex_u8(
        encoded, offset, static_cast<std::uint8_t>(hostname.size()));
    for(std::size_t index = 0U;
        index < application::maximum_github_hostname_bytes;
        ++index) {
        const std::uint8_t value =
            index < hostname.size()
            ? static_cast<std::uint8_t>(
                  static_cast<unsigned char>(hostname[index]))
            : std::uint8_t{0U};
        append_hex_u8(encoded, offset, value);
    }

    error_owner error;
    Service = secret_service_open_sync(
        non_prompting_secret_service_get_type(),
        c_str(canonical_service_bus_name),
        SECRET_SERVICE_NONE,
        nullptr,
        error.out());
    if(Service == nullptr) {
        const auto failure = mapped_error(error.get());
        return std::unexpected{
            failure
                    == application::github_credential_vault_failure::
                        operation_failed
                ? application::github_credential_vault_failure::unavailable
                : failure};
    }
    const auto* actual_name =
        g_dbus_proxy_get_name(G_DBUS_PROXY(Service));
    if(actual_name == nullptr
        || std::strcmp(
               actual_name,
               c_str(canonical_service_bus_name))
            != 0) {
        g_object_unref(Service);
        Service = nullptr;
        return std::unexpected{
            application::github_credential_vault_failure::unavailable};
    }
    error_owner session_error;
    if(!secret_service_ensure_session_sync(
           Service,
           nullptr,
           session_error.out())) {
        const auto failure = mapped_error(session_error.get());
        g_object_unref(Service);
        Service = nullptr;
        return std::unexpected{
            failure
                    == application::github_credential_vault_failure::
                        operation_failed
                ? application::github_credential_vault_failure::unavailable
                : failure};
    }

    auto* collection = secret_collection_for_alias_sync(
        Service,
        SECRET_COLLECTION_DEFAULT,
        SECRET_COLLECTION_NONE,
        nullptr,
        nullptr);
    secret_collection_get_locked(collection);
    secret_item_get_locked(item);

    auto* stored_value = secret_value_new(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<gssize>(bytes.size()),
        c_str(binary_content_type));
    secret_item_create_sync(
        collection,
        &schema,
        attributes,
        label,
        stored_value,
        SECRET_ITEM_CREATE_REPLACE,
        nullptr,
        nullptr);
    secret_collection_search_sync(
        collection,
        &schema,
        attributes,
        SECRET_SEARCH_ALL,
        nullptr,
        nullptr);
    secret_item_load_secret_sync(item, nullptr, nullptr);
    auto* value = secret_item_get_secret(item);
    const auto* content_type =
        value == nullptr ? nullptr : secret_value_get_content_type(value);
    const auto recognized_content_type =
        content_type != nullptr
        && (
            std::strcmp(content_type, c_str(binary_content_type)) == 0
            || std::strcmp(
                   content_type,
                   c_str(gnome_keyring_content_type))
                == 0);
    if(value == nullptr || content_type == nullptr
        || !recognized_content_type) {}
    secret_value_get(value, &size);
    static_assert(application::maximum_github_credential_bytes == 2'560U);
    secret_item_delete_sync(item, nullptr, nullptr);
    secret_value_unref(stored_value);
}
"""


class ArchitectureBoundaryTests(unittest.TestCase):
    def run_check(
        self,
        sources: dict[str, str] | None = None,
        *,
        adapter_root: str = ADAPTER_ROOT,
        github_target: str = GITHUB_TARGET,
        git_target: str = GIT_TARGET,
        agent_target: str = AGENT_TARGET,
        omit_macos_source: bool = False,
        omit_windows_source: bool = False,
        omit_linux_source: bool = False,
        omit_process_interface: bool = False,
        omit_process_runner_interface: bool = False,
        omit_process_runner: bool = False,
        omit_cli_transport: bool = False,
        omit_paths: tuple[str, ...] = (),
    ) -> subprocess.CompletedProcess[str]:
        with contained_temporary_directory() as temporary_directory:
            root = Path(temporary_directory)
            files = {
                "CMakeLists.txt": "",
                "cmake/BprDependencies.cmake": DEPENDENCIES_TARGET,
                "src/core/CMakeLists.txt":
                    "add_library(better_pull_review_core "
                    "src/agent_profile.cpp)\n",
                "src/application/CMakeLists.txt": "",
                "src/adapters/CMakeLists.txt": adapter_root,
                "src/adapters/github/CMakeLists.txt": github_target,
                "src/adapters/git/CMakeLists.txt": git_target,
                "src/adapters/agent/CMakeLists.txt": agent_target,
                "src/adapters/qt/CMakeLists.txt": "",
                "src/adapters/git/src/installed_git_process.cpp":
                    GIT_PROCESS_RUNNER_SOURCE,
                PROCESS_SUPPORT_CMAKE_RELATIVE: PROCESS_SUPPORT_CMAKE,
                PROCESS_SUPPORT_SOURCE_RELATIVE: PROCESS_SUPPORT_SOURCE,
                PROCESS_SUPPORT_HEADER_RELATIVE: PROCESS_SUPPORT_HEADER,
                "src/application/include/bpr/application/"
                "git_process_port.hpp": GIT_PROCESS_PORT,
                "src/adapters/git/include/bpr/adapters/git/"
                "installed_git_process.hpp": GIT_PROCESS_FACTORY,
                "src/adapters/agent/src/agent_process_launcher.cpp":
                    AGENT_PROCESS_RUNNER_SOURCE,
                "src/adapters/agent/src/agent_protocol_loop.cpp":
                    AGENT_PROTOCOL_LOOP_SOURCE,
                "src/adapters/agent/src/windows_agent_process.cpp":
                    AGENT_WINDOWS_PROCESS_SOURCE,
                "src/adapters/agent/src/unix_agent_supervisor.cpp":
                    (REPOSITORY_ROOT / "src/adapters/agent/src/unix_agent_supervisor.cpp").read_text(),
                "src/adapters/agent/src/windows_agent_supervisor.cpp":
                    AGENT_WINDOWS_SUPERVISOR_SOURCE,
            }
            if not omit_process_interface:
                files[
                    "src/adapters/github/src/github_cli_process.hpp"
                ] = GITHUB_CLI_PROCESS_INTERFACE
            if not omit_process_runner_interface:
                files[
                    "src/adapters/github/src/github_cli_process_runner.hpp"
                ] = GITHUB_CLI_PROCESS_RUNNER_INTERFACE
            if not omit_process_runner:
                files[
                    "src/adapters/github/src/github_cli_process_runner.cpp"
                ] = GITHUB_CLI_PROCESS_RUNNER_SOURCE
            if not omit_cli_transport:
                files[
                    "src/adapters/github/src/gh_cli_transport.cpp"
                ] = GH_CLI_TRANSPORT_SOURCE
            files[
                "src/adapters/github/src/native_https_transport.hpp"
            ] = NATIVE_HTTPS_TRANSPORT_INTERFACE
            files[
                "src/adapters/github/src/native_https_transport.cpp"
            ] = NATIVE_HTTPS_TRANSPORT_SOURCE
            files[
                "src/adapters/github/src/github_wire_request_validation.hpp"
            ] = GITHUB_WIRE_VALIDATION_INTERFACE
            files[
                "src/adapters/github/src/github_wire_request_validation.cpp"
            ] = GITHUB_WIRE_VALIDATION_SOURCE
            if not omit_macos_source:
                files["src/adapters/github/src/macos_keychain.cpp"] = (
                    MACOS_KEYCHAIN_SOURCE
                )
            if not omit_windows_source:
                files[
                    "src/adapters/github/src/windows_credential_manager.cpp"
                ] = WINDOWS_CREDENTIAL_MANAGER_SOURCE
            if not omit_linux_source:
                files[
                    "src/adapters/github/src/linux_secret_service.cpp"
                ] = LINUX_SECRET_SERVICE_SOURCE
            files.update(sources or {})
            for omitted in omit_paths:
                files.pop(omitted)

            for relative_path, contents in files.items():
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                # `newline=""` writes the literal bytes. Without it Python
                # translates every `\n` to `\r\n` on Windows, and the checker
                # this stages for compares the designated-live canary surface
                # against a SHA-256 taken over the LF form — so the fixture
                # failed as "not exact" for a reason that had nothing to do with
                # what the case was testing.
                path.write_text(contents, encoding="utf-8", newline="")

            return subprocess.run(
                [
                    shutil.which("cmake") or "cmake",
                    f"-DBPR_ROOT={root}",
                    "-P",
                    str(VERIFY_SCRIPT),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

    def assert_failure(
        self,
        expected_message: str,
        sources: dict[str, str] | None = None,
        **kwargs: object,
    ) -> None:
        result = self.run_check(sources, **kwargs)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(expected_message, result.stdout + result.stderr)

    def test_designated_github_effect_sources_are_allowed(self) -> None:
        result = self.run_check(
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE,
                "src/adapters/github/src/gh_cli_transport.cpp":
                    GH_CLI_TRANSPORT_SOURCE,
                "src/adapters/github/src/native_https_transport.cpp":
                    NATIVE_HTTPS_TRANSPORT_SOURCE,
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tour_editor_has_one_owner_and_explicit_trusted_argv(self) -> None:
        path = "src/app/src/tour_editor.cpp"
        source = (REPOSITORY_ROOT / path).read_text(encoding="utf-8")
        sources = {path: source, "src/app/CMakeLists.txt":
                   "target_sources(better_pull_review_tour_runtime PRIVATE src/tour_editor.cpp)\n"}
        result = self.run_check(sources)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_failure("QProcess ownership violation", {
            "src/app/src/other_editor.cpp": "QProcess process;\n"})
        for primitive in ("startCommand", "splitCommand", "setNativeArguments", "startDetached", "QProcess::execute", "QSettings"):
            with self.subTest(primitive=primitive):
                changed = dict(sources, **{path: source + primitive + ";\n"})
                self.assert_failure("Tour editor must retain explicit trusted argv", changed)
        for boundary in ("BPR_TOUR_EDITOR_ARGV", "CloseFileDescriptors", "O_NOFOLLOW",
                         "FILE_FLAG_OPEN_REPARSE_POINT", "inheritance.apply", "process.waitForFinished"):
            with self.subTest(boundary=boundary):
                changed = dict(sources, **{path: source.replace(boundary, "removed_boundary")})
                self.assert_failure("Tour editor lost its " + boundary + " boundary", changed)

    def test_qt_adapter_local_hiding_a_qwidget_data_member_is_rejected(self) -> None:
        # [[BPR-1343]]. Each declaration form that hides QWidget::data or
        # QObject::d_ptr breaks the MSVC build.
        for source in (
            "\t\tconst auto data = Hook->currentData();\n",
            "QVariant data = value;\n",
            "void show(const QByteArray& data) {}\n",
            "for (const auto& data : items) {}\n",
            "auto task = [this, data] { };\n",
            "auto task = [&data]() { };\n",
            "auto* d_ptr = nullptr;\n",
        ):
            with self.subTest(source=source):
                self.assert_failure(
                    "Qt adapter declaration hides a Qt base class data member",
                    {"src/adapters/qt/src/shadowing_widget.cpp": source},
                )

    def test_qt_adapter_uses_of_data_that_hide_nothing_are_allowed(self) -> None:
        # [[BPR-1343]]. A model override, calls, comments, strings, member
        # access, a qualified name and the coding standard's `aData` hide
        # nothing.
        result = self.run_check(
            {
                "src/adapters/qt/src/shadowing_widget.cpp": (
                    "auto data(const QModelIndex& aIndex, const int aRole) const -> QVariant override;\n"
                    "Status->setText(QString::fromUtf8(aText.data(), 3));\n"
                    "// keep the data, then the rest; data = x\n"
                    "auto s = QStringLiteral(\"the data, then\"); auto t = \"data = 1;\";\n"
                    "auto v = ptr->data;\n"
                    "auto w = data::value;\n"
                    "void f(const QByteArray& aData) {}\n"
                    "const auto metadata = x;\n"
                ),
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_qprocess_is_rejected_outside_designated_runner(self) -> None:
        self.assert_failure(
            "QProcess ownership violation",
            {"src/adapters/git/process.cpp": "QProcess* process;\n"},
        )

    def test_designated_installed_git_process_runner_is_allowed(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_native_release_staging_effect_owner_is_exact(self) -> None:
        source = "QProcess process;\nSecStaticCodeRef code;\n"
        target = (
            "target_sources(better_pull_review_adapter_update PRIVATE "
            "src/macos_release_update.cpp)\n"
        )
        result = self.run_check({
            "src/adapters/update/src/macos_release_update.cpp": source,
            "src/adapters/update/CMakeLists.txt": target,
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for expression, diagnostic in (
            ("QProcess process;\n", "QProcess ownership violation"),
            ("SecStaticCodeRef code;\n", "macOS code-signing API ownership violation"),
        ):
            with self.subTest(expression=expression):
                self.assert_failure(diagnostic, {
                    "src/adapters/update/src/other.cpp": expression,
                })

    def test_sha1_identities_have_one_collision_detecting_owner(self) -> None:
        # [[BPR-1450]]. Qt's plain SHA-1 serves only the pack checksum, and
        # only the core's sha1collisiondetection wrapper names a SHA-1 API.
        result = self.run_check({
            "src/adapters/git/src/installed_git_cache.cpp":
                "QCryptographicHash Hash{ QCryptographicHash::Sha1 };\n",
            "src/core/src/sha1_collision_detecting.cpp":
                "#include <sha1.h>\nSHA1_CTX context;\nSHA1DCInit(&context);\n",
            "src/core/CMakeLists.txt":
                "add_library(better_pull_review_core src/agent_profile.cpp src/sha1_collision_detecting.cpp)\n",
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression, diagnostic in (
            ("src/adapters/github/src/gh_cli_git_smart_transport.cpp",
             "QCryptographicHash hash{ QCryptographicHash::Sha1 };\n", "plain Qt SHA-1"),
            ("src/application/src/epoch_application_host.cpp",
             "QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);\n", "plain Qt SHA-1"),
            ("src/core/src/other_sha1.cpp", "std::uint32_t h0 = 0x67452301U;\n", "SHA-1 implementation ownership violation"),
            ("src/app/src/other.mm", "CC_SHA1_Init(&context);\n", "SHA-1 implementation ownership violation"),
            ("src/adapters/git/src/other.cpp", "#include <sha1.h>\n", "SHA-1 implementation ownership violation"),
            ("src/adapters/git/src/other.cpp", "SHA1DCUpdate(&context, data, size);\n", "SHA-1 implementation ownership violation"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure(diagnostic, {path: expression})

    def test_sha256_has_one_hacl_owner(self) -> None:
        # [[BPR-1451]]. Only the core wrapper calls HACL*, and no first-party
        # SHA-2 constants return anywhere else.
        result = self.run_check({
            "src/core/src/sha256_hacl.cpp":
                "#include <internal/Hacl_Hash_SHA2.h>\nHacl_Hash_SHA2_sha256_init(state);\n",
            "src/core/CMakeLists.txt":
                "add_library(better_pull_review_core src/agent_profile.cpp src/sha256_hacl.cpp)\n",
            "src/adapters/sqlite/src/other.cpp":
                "QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);\n",
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression in (
            ("src/core/src/other_sha256.cpp", "#include <internal/Hacl_Hash_SHA2.h>\n"),
            ("src/application/src/other.cpp", "Hacl_Hash_SHA2_sha256_update_nblocks(64U, block, state);\n"),
            ("src/core/src/other_sha256.cpp", "std::uint32_t h0 = 0x6a09e667U;\n"),
            ("src/adapters/git/src/other.cpp", "constexpr auto k0 = 0x428A2F98U;\n"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure("SHA-256 implementation ownership violation", {path: expression})

    def test_line_pair_similarity_has_one_rapidfuzz_owner(self) -> None:
        # [[BPR-1480]]. Only the engine's pair scoring names rapidfuzz-cpp.
        result = self.run_check({
            "src/core/src/diff_compute.cpp":
                "#include <rapidfuzz/distance/Indel.hpp>\n"
                "std::optional<rapidfuzz::CachedIndel<std::uint32_t>> Cached;\n",
            "src/core/CMakeLists.txt":
                "add_library(better_pull_review_core src/agent_profile.cpp src/diff_compute.cpp)\n",
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression in (
            ("src/core/src/diff_intraline.cpp", "#include <rapidfuzz/distance/Indel.hpp>\n"),
            ("src/core/src/fuzzy_match.cpp", "rapidfuzz::fuzz::ratio(left, right);\n"),
            ("src/adapters/qt/src/other.cpp", "#define RAPIDFUZZ_SIMD 1\n"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure("rapidfuzz-cpp line-pair similarity ownership violation", {path: expression})

    def test_linked_xdiff_has_one_adapter_owner(self) -> None:
        # [[BPR-1485]]. Only the xdiff adapter calls xdiff and links its shared
        # library, which only the provider builds, and only as SHARED.
        adapter = {
            "src/adapters/xdiff/src/xdiff_line_matcher.cpp":
                'extern "C" {\n#include "xdiff.h"\n}\nauto parameters = xpparam_t{ };\nparameters.flags = XDF_INDENT_HEURISTIC;\n'
                "xdl_diff(&left, &right, &parameters, &configuration, &callback);\n",
            "src/adapters/xdiff/CMakeLists.txt":
                "add_library(better_pull_review_adapter_xdiff STATIC src/xdiff_line_matcher.cpp)\n"
                "target_link_libraries(better_pull_review_adapter_xdiff PUBLIC BetterPullReview::Application PRIVATE BetterPullReview::Xdiff)\n",
            "cmake/BprXdiffProvider.cmake":
                "add_library(better_pull_review_xdiff SHARED ${source}/xdiff/xdiffi.c)\n",
        }
        result = self.run_check(adapter)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression in (
            ("src/core/src/diff_xdiff.cpp", '#include "xdiff.h"\n'),
            ("src/application/src/diff_line_matcher.cpp", "xdl_diff(&left, &right, &parameters, &configuration, &callback);\n"),
            ("src/adapters/git/src/other.cpp", "mmfile_t left{ };\n"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure("linked xdiff ownership violation", {**adapter, path: expression})
        for path, expression, diagnostic in (
            ("cmake/BprXdiffProvider.cmake", "add_library(better_pull_review_xdiff STATIC ${source}/xdiff/xdiffi.c)\n",
             "xdiff must be built only as its own shared library"),
            ("src/adapters/qt/CMakeLists.txt", "target_link_libraries(better_pull_review_adapter_qt PRIVATE BetterPullReview::Xdiff)\n",
             "xdiff ownership violation in src/adapters/qt/CMakeLists.txt"),
            ("src/core/CMakeLists.txt", "add_library(better_pull_review_core src/agent_profile.cpp ${git}/xdiff/xdiffi.c)\n",
             "xdiff ownership violation in src/core/CMakeLists.txt"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure(diagnostic, {**adapter, path: expression})

    def test_dtl_has_exactly_two_owners(self) -> None:
        # [[BPR-1481]] [[BPR-1498]]. Only the engine's intraline comparison
        # and the Review Cycle suggestion hunks name dtl, so it never becomes
        # the diff engine's whole-file line matcher.
        owners = {
            "src/core/src/diff_intraline.cpp":
                "#include <dtl/dtl.hpp>\n"
                "auto diff = dtl::Diff<std::uint32_t>{ left, right };\n",
            "src/core/src/review_cycle_suggestion.cpp":
                "#include <dtl/dtl.hpp>\n"
                "static_assert(points < dtl::MAX_CORDINATES_SIZE);\n",
            "src/core/CMakeLists.txt":
                "add_library(better_pull_review_core src/agent_profile.cpp src/diff_intraline.cpp src/review_cycle_suggestion.cpp)\n",
        }
        result = self.run_check(owners)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for owner in ("src/core/src/diff_intraline.cpp", "src/core/src/review_cycle_suggestion.cpp"):
            with self.subTest(owner=owner):
                alone = {path: text for path, text in owners.items() if path == owner or path.endswith("CMakeLists.txt")}
                result = self.run_check(alone)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression in (
            ("src/core/src/review_cycle_comparison.cpp", "#include <dtl/dtl.hpp>\n"),
            ("src/core/src/diff_semantics.cpp", "#include <dtl/dtl.hpp>\n"),
            ("src/core/src/diff_line_matcher.cpp", "auto lines = dtl::Diff<line_class>{ left, right };\n"),
            ("src/adapters/xdiff/src/matcher.cpp", "if (info.type == dtl::SES_COMMON) {}\n"),
            ("src/application/src/diff_line_matcher.cpp", "#ifdef DTL_VARIABLES_H\n#endif\n"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure("dtl comparison ownership violation", {**owners, path: expression})

    def test_platform_sha256_has_exact_native_and_adapter_owners(self) -> None:
        result = self.run_check({
            "src/adapters/github/src/platform_body_digest.cpp":
                "#include <CommonCrypto/CommonDigest.h>\nCC_SHA256(data, size, digest);\n",
            "src/core/src/sha256_commoncrypto.cpp":
                "#include <CommonCrypto/CommonDigest.h>\nCC_SHA256_Update(&context, data, size);\n",
            "src/core/src/sha256_cng.cpp":
                "BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &handle, nullptr, 0, nullptr, 0, 0);\n",
            "src/core/CMakeLists.txt":
                "add_library(better_pull_review_core src/agent_profile.cpp src/sha256_commoncrypto.cpp src/sha256_cng.cpp)\n",
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, expression in (
            ("src/core/src/feedback_convention.cpp", "#include <CommonCrypto/CommonDigest.h>\n"),
            ("src/application/src/body_digest_port.cpp", "CC_SHA256(data, size, digest);\n"),
            ("src/application/src/feedback_sources.cpp", "CC_SHA256_Init(&context);\n"),
            ("src/adapters/github/src/github_provider.cpp", "CC_SHA512(data, size, digest);\n"),
            ("src/app/src/other.mm", "CC_SHA256_Update(&context, data, size);\n"),
        ):
            with self.subTest(path=path, expression=expression):
                self.assert_failure("platform crypto (CommonCrypto SHA-2) ownership violation", {path: expression})

        for path in ("src/core/src/other.cpp", "src/application/src/other.cpp",
                     "src/adapters/github/src/platform_body_digest.cpp"):
            with self.subTest(path=path):
                self.assert_failure("platform crypto (CNG SHA-256) ownership violation",
                                    {path: "BCryptHashData(handle, bytes, size, 0);\n"})

    def test_native_release_helper_launch_owner_is_exact(self) -> None:
        source = "posix_spawn(pid, path, actions, attr, argv, envp);\n"
        result = self.run_check({
            "src/adapters/update/src/macos_update_helper.cpp": source,
            "src/adapters/update/CMakeLists.txt": (
                "target_sources(better_pull_review_adapter_update PRIVATE "
                "src/macos_update_helper.cpp)\n"
            ),
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_failure("posix_spawn ownership violation", {
            "src/adapters/update/src/other.cpp": source,
        })

    def test_native_windows_agent_process_owner_is_exact(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        self.assert_failure(
            "native Windows process creation API ownership violation",
            {
                "src/adapters/agent/src/other.cpp":
                    "CreateProcessW(nullptr, nullptr, nullptr, nullptr, "
                    "FALSE, 0U, nullptr, nullptr, nullptr, nullptr);\n"
            },
        )
        self.assert_failure(
            "native Windows agent containment API ownership violation",
            {
                "src/adapters/agent/src/other.cpp":
                    "AssignProcessToJobObject(nullptr, nullptr);\n"
            },
        )

    def test_terminal_cancellation_owners_cannot_acquire_containment(self) -> None:
        owners = (
            "src/adapters/mcp/src/mcp_terminal_diagnostic.hpp",
            "src/app/src/terminal_diagnostics.cpp",
        )
        sources = {path: "CancelSynchronousIo(writer);\n" for path in owners}
        sources["src/adapters/mcp/CMakeLists.txt"] = (
            "target_sources(tool_better_pull_review_mcp_context_helper PRIVATE src/mcp_terminal_diagnostic.hpp)\n"
        )
        sources["src/app/CMakeLists.txt"] = "target_sources(better_pull_review_composition PRIVATE src/terminal_diagnostics.cpp)\n"
        result = self.run_check(sources)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path in owners:
            for primitive in ("AssignProcessToJobObject", "PROC_THREAD_ATTRIBUTE_HANDLE_LIST"):
                with self.subTest(path=path, primitive=primitive):
                    self.assert_failure(
                        "native Windows agent containment API ownership violation",
                        {path: primitive + ";\n"},
                    )
        self.assert_failure(
            "native Windows synchronous I/O cancellation API ownership violation",
            {"src/adapters/mcp/src/other.cpp": "CancelSynchronousIo(writer);\n"},
        )

    def test_native_windows_agent_target_owns_selected_backend(self) -> None:
        self.assert_failure(
            "Agent process adapter target contract is incomplete",
            agent_target=AGENT_TARGET.replace(
                "            src/windows_agent_process.cpp\n",
                "",
            ),
        )
        # [[BPR-0842]] Only the adapter's link goes. The Windows supervisor
        # keeps its own `Kernel32`, which an unscoped pattern accepted.
        adapter_private = "        better_pull_review_adapter_agent\n        PRIVATE\n"
        adapter_link = adapter_private + "            Kernel32\n"
        self.assertEqual(AGENT_TARGET.count(adapter_link), 1)
        self.assert_failure(
            "Agent process adapter target contract is incomplete",
            agent_target=AGENT_TARGET.replace(adapter_link, adapter_private),
        )

    def test_windows_agent_supervisor_owns_process_and_job_apis(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        # The same source under another name is refused for each API family,
        # so the permission is the designated file's and it is exercised.
        other = "src/adapters/agent/src/other_agent_supervisor.cpp"
        self.assert_failure(
            "native Windows process creation API ownership violation",
            {other: AGENT_WINDOWS_SUPERVISOR_SOURCE},
        )
        self.assert_failure(
            "native Windows agent containment API ownership violation",
            {
                other: AGENT_WINDOWS_SUPERVISOR_SOURCE.replace(
                    "CreateProcessW",
                    "RemovedProcessCreation",
                )
            },
        )

    def test_windows_private_descriptor_has_one_definition(self) -> None:
        # [[BPR-0825]] Only the platform header builds the Windows
        # owner-private descriptor. A second builder, in the SQLite storage
        # root or the agent's workspace guard as before, is refused, and the
        # permission is the platform target's own header.
        builder = "InitializeAcl(acl, size, ACL_REVISION);\n"
        platform = {
            PLATFORM_HEADER: builder,
            "src/platform/CMakeLists.txt": PLATFORM_TARGET,
        }
        result = self.run_check(platform)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for other, call in (
            ("src/adapters/sqlite/src/catalog_database.cpp", builder),
            (
                "src/adapters/agent/include/bpr/adapters/agent/"
                "windows_workspace_guard.hpp",
                "AddAccessAllowedAceEx(acl, ACL_REVISION, 0, FILE_ALL_ACCESS, sid);\n",
            ),
            (
                "src/adapters/native/windows_private_storage.hpp",
                "SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE);\n",
            ),
        ):
            with self.subTest(other=other):
                self.assert_failure(
                    "Windows owner-private descriptor construction ownership "
                    "violation",
                    {**platform, other: call},
                )
        self.assert_failure(
            # CMake wraps its message before the owner's name.
            "Designated effect source windows_private_object.hpp is not owned",
            {
                PLATFORM_HEADER: builder,
                "src/platform/CMakeLists.txt":
                    "add_library(better_pull_review_platform INTERFACE)\n",
            },
        )

    def test_platform_target_stays_neutral(self) -> None:
        # [[BPR-0825]] The platform target is the neutral home the owner chose
        # over an adapter-to-adapter edge, so it may not take one itself.
        self.assert_failure(
            "Platform target must not depend on a first-party layer, Qt or a "
            "provider",
            {
                PLATFORM_HEADER: "",
                "src/platform/CMakeLists.txt": PLATFORM_TARGET
                + "target_link_libraries(better_pull_review_platform "
                "INTERFACE BetterPullReview::AdapterSQLite)\n",
            },
        )
        for include in (
            "#include <QString>\n",
            '#include "bpr/adapters/sqlite/storage_root.hpp"\n',
            '#include "bpr/application/git_process_port.hpp"\n',
        ):
            with self.subTest(include=include):
                self.assert_failure(
                    "Platform target must stay free of Qt, providers",
                    {
                        PLATFORM_HEADER: include,
                        "src/platform/CMakeLists.txt": PLATFORM_TARGET,
                    },
                )
        self.assert_failure(
            "Platform target must be the header-only "
            "better_pull_review_platform",
            {
                PLATFORM_HEADER: "",
                "src/platform/CMakeLists.txt":
                    "add_library(better_pull_review_platform STATIC "
                    "include/bpr/platform/windows_private_object.hpp)\n",
            },
        )

    def test_agent_backends_select_one_shared_protocol_loop(self) -> None:
        self.assert_failure(
            "Agent process adapter target contract is incomplete",
            agent_target=AGENT_TARGET.replace(
                "        src/agent_protocol_loop.cpp\n",
                "",
            ),
        )
        for backend in (
            "src/adapters/agent/src/agent_process_launcher.cpp",
            "src/adapters/agent/src/windows_agent_process.cpp",
        ):
            with self.subTest(backend=backend):
                source = (
                    AGENT_PROCESS_RUNNER_SOURCE
                    if backend.endswith("agent_process_launcher.cpp")
                    else AGENT_WINDOWS_PROCESS_SOURCE
                )
                self.assert_failure(
                    "Agent process backend must select the shared protocol loop",
                    {backend: source.replace(
                        "run_agent_protocol_loop",
                        "removed_shared_protocol_loop",
                    )},
                )
                self.assert_failure(
                    "Agent process backend duplicates shared protocol logic",
                    {backend: source + "\nagent_protocol_decoder duplicate;\n"},
                )

    def test_posix_agent_transport_preserves_exit_first_bounded_batches(
        self,
    ) -> None:
        self.assertNotIn("readAllStandardOutput", AGENT_PROCESS_RUNNER_SOURCE)
        self.assertNotIn("readAllStandardError", AGENT_PROCESS_RUNNER_SOURCE)
        for bounded_read_part in (
            "setReadChannel",
            "aBatchRemaining = State.Process.bytesAvailable()",
            "State.Process.read(aBuffer.data(), requested)",
            ".BatchComplete = aBatchRemaining == 0",
        ):
            with self.subTest(bounded_read_part=bounded_read_part):
                self.assertIn(
                    bounded_read_part,
                    AGENT_PROCESS_RUNNER_SOURCE,
                )

        wait_start = AGENT_PROCESS_RUNNER_SOURCE.index(
            "[[nodiscard]] auto wait()"
        )
        wait_end = AGENT_PROCESS_RUNNER_SOURCE.index(
            "[[nodiscard]] auto terminate()",
            wait_start,
        )
        wait_body = AGENT_PROCESS_RUNNER_SOURCE[wait_start:wait_end]
        self.assertLess(
            wait_body.index("QProcess::NotRunning"),
            wait_body.index("Cancelled()"),
        )
        self.assertLess(
            wait_body.index("QProcess::NotRunning"),
            wait_body.index("Elapsed.elapsed()"),
        )

    def test_windows_agent_transport_preserves_bounded_batches(self) -> None:
        for bounded_read_part in (
            "aBatchRemaining = static_cast<std::size_t>(available)",
            "std::min(aBuffer.size(), aBatchRemaining)",
            "aBatchRemaining -= static_cast<std::size_t>(read)",
            ".BatchComplete = aBatchRemaining == 0U",
        ):
            with self.subTest(bounded_read_part=bounded_read_part):
                self.assertIn(
                    bounded_read_part,
                    AGENT_WINDOWS_PROCESS_SOURCE,
                )

    def test_native_windows_agent_requires_suspended_containment_sequence(
        self,
    ) -> None:
        for required in (
            "CreateProcessW",
            "CREATE_SUSPENDED",
            "PROC_THREAD_ATTRIBUTE_HANDLE_LIST",
            "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE",
            "AssignProcessToJobObject",
            "ResumeThread",
            "TerminateJobObject",
            "CancelSynchronousIo",
            "QueryInformationJobObject",
            "ForceJobAssignmentFailure",
        ):
            with self.subTest(required=required):
                self.assert_failure(
                    "Native Windows agent process contract is incomplete",
                    {
                        "src/adapters/agent/src/windows_agent_process.cpp":
                            AGENT_WINDOWS_PROCESS_SOURCE.replace(
                                required,
                                "REMOVED_REQUIRED_NATIVE_PROCESS_PRIMITIVE",
                            )
                    },
                )

    def test_native_windows_agent_assigns_job_before_resume(self) -> None:
        # The pre-resume claim's success path: after the suspended creation,
        # the job assignment must precede the resume in source order, so a
        # reorder cannot open an unassigned execution window (BPR-0240).
        launch_body = AGENT_WINDOWS_PROCESS_SOURCE[
            AGENT_WINDOWS_PROCESS_SOURCE.index("CREATE_SUSPENDED"):
        ]
        self.assertLess(
            launch_body.index("assign_process_to_job"),
            launch_body.index("ResumeThread"),
        )

    def test_native_windows_agent_rejects_escape_and_fallback_surfaces(
        self,
    ) -> None:
        hostile_surfaces = (
            "CreateProcessA(nullptr, nullptr, nullptr, nullptr, FALSE, 0U, "
            "nullptr, nullptr, nullptr, nullptr);\n",
            "const DWORD escape = CREATE_BREAKAWAY_FROM_JOB;\n",
            "const DWORD escape = JOB_OBJECT_LIMIT_BREAKAWAY_OK;\n",
            "auto* inherited = GetEnvironmentStringsW();\n",
            "QProcess fallback;\n",
            "ShellExecuteW(nullptr, nullptr, nullptr, nullptr, nullptr, 0);\n",
        )
        for hostile_surface in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    "Forbidden native Windows agent process surface",
                    {
                        "src/adapters/agent/src/windows_agent_process.cpp":
                            AGENT_WINDOWS_PROCESS_SOURCE + hostile_surface
                    },
                )

    def test_core_agent_profile_owns_only_agent_environment_policy_names(
        self,
    ) -> None:
        policy = (
            'constexpr const char* agent_environment_policy[] = '
            '{"HOME", "PATH"};\n'
        )
        result = self.run_check({"src/core/src/agent_profile.cpp": policy})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        for neighboring_source in (
            "src/core/src/other.cpp",
            "src/application/src/agent_profile.cpp",
        ):
            with self.subTest(neighboring_source=neighboring_source):
                self.assert_failure(
                    "process environment construction ownership violation",
                    {neighboring_source: policy},
                )

        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {
                "src/core/src/agent_profile.cpp":
                    policy + "QProcess* process;\n"
            },
        )

    def test_git_qprocess_environment_is_rejected_in_a_second_file(
        self,
    ) -> None:
        self.assert_failure(
            "QProcess ownership violation",
            {
                "src/adapters/git/src/other.cpp":
                    "QProcessEnvironment environment;\n"
            },
        )

    def test_installed_git_target_must_own_designated_runner(self) -> None:
        self.assert_failure(
            "Installed-Git adapter target contract is incomplete",
            git_target=GIT_TARGET.replace(
                "            src/installed_git_process.cpp\n",
                "",
            ),
        )

    def assert_process_support_failure(
        self,
        expected: str,
        relative: str,
        original: str,
        mutation: str,
    ) -> None:
        contents = {
            PROCESS_SUPPORT_CMAKE_RELATIVE: PROCESS_SUPPORT_CMAKE,
            PROCESS_SUPPORT_SOURCE_RELATIVE: PROCESS_SUPPORT_SOURCE,
            PROCESS_SUPPORT_HEADER_RELATIVE: PROCESS_SUPPORT_HEADER,
        }[relative]
        self.assertIn(original, contents)
        self.assert_failure(
            expected,
            {relative: contents.replace(original, mutation, 1)},
        )

    def test_shared_process_supervision_contract(self) -> None:
        # [[BPR-1436]] The gh and Git runners' containment rests on this one
        # target, so losing any part of it is refused.
        self.assertEqual(self.run_check().returncode, 0)
        header = PROCESS_SUPPORT_HEADER_RELATIVE
        source = PROCESS_SUPPORT_SOURCE_RELATIVE
        cmake = PROCESS_SUPPORT_CMAKE_RELATIVE

        for relative, expected in (
            # Without its CMake file, no target owns the QProcess source.
            (cmake, "is not owned by"),
            (source, "Shared process supervision is missing"),
            (header, "Shared process supervision is missing"),
        ):
            with self.subTest(missing=relative):
                self.assert_failure(expected, omit_paths=(relative,))
        retained = "must retain its process-group kill"
        for relative, fragment in (
            (header, "class serialized_process_lane final"),
            (header, "~serialized_process_lane()"),
            (header, "ShutdownRequested.store("),
            (header, "Worker.join();"),
            (header, "while (Busy)"),
            (header, "WorkAvailable.notify_one();"),
            (source, "-static_cast<pid_t>(aProcessGroupId)"),
            (source, "::kill(processGroup, SIGKILL)"),
            (source, "return processId > 1 ? processId : 0;"),
            (source, "aProcess.kill();"),
            (source, "aProcess.release()"),
        ):
            with self.subTest(retained=fragment):
                self.assert_process_support_failure(
                    retained, relative, fragment, "REMOVED_SHARED_SUPERVISION"
                )

        self.assert_process_support_failure(
            "exactly one worker thread",
            header,
            "\t\tTTraits ",
            "\t\tstd::thread Second;\n\t\tTTraits ",
        )
        self.assert_process_support_failure(
            "Shared bounded reap must resolve",
            source,
            "\t\t\tstatic_cast<void>(aProcess.waitForFinished(wait));\n"
            "\t\t\tprocessGroupId = resolve_process_group(aProcess, processGroupId);\n"
            "\t\t\taKillTree(aProcess, processGroupId);\n",
            "\t\t\tstatic_cast<void>(aProcess.waitForFinished(wait));\n"
            "\t\t\taKillTree(aProcess, processGroupId);\n",
        )
        # The gh runner's background reap had no deadline before BPR-1436.
        self.assert_process_support_failure(
            "background reap must stop at its deadline",
            source,
            "\t\t\tconst auto wait = wait_milliseconds(deadline);\n"
            "\t\t\tif (wait == 0) {\n"
            "\t\t\t\treturn false;\n"
            "\t\t\t}\n"
            "\t\t\tprocessGroupId = resolve_process_group(aProcess, processGroupId);",
            "\t\t\tconst auto wait = static_cast<int>(wait_slice.count());\n"
            "\t\t\tprocessGroupId = resolve_process_group(aProcess, processGroupId);",
        )
        self.assert_process_support_failure(
            "Shared process lane must publish",
            header,
            "Quarantined = !reaped;",
            "Quarantined = false;",
        )
        self.assert_process_support_failure(
            "Shared process lane must publish",
            header,
            "abandon_unreaped_process(execution->AwaitingReap);",
            "execution->AwaitingReap.reset();",
        )
        publish = "\t\t\t\tpublish(item, std::move(execution->Result));\n"
        quarantine = (
            "\t\t\t\tif (execution->AwaitingReap != nullptr) {\n"
            "\t\t\t\t\t{\n"
            "\t\t\t\t\t\tstd::lock_guard lock{ Mutex };\n"
            "\t\t\t\t\t\tif (!Quarantined) {\n"
            "\t\t\t\t\t\t\tobserve(*item, lane_observation::quarantined);\n"
            "\t\t\t\t\t\t}\n"
            "\t\t\t\t\t\tQuarantined = true;\n"
            "\t\t\t\t\t}\n"
            "\t\t\t\t\tSlotAvailable.notify_all();\n"
            "\t\t\t\t}\n"
        )
        self.assert_process_support_failure(
            "Shared process lane must publish",
            header,
            quarantine + publish,
            publish + quarantine,
        )
        self.assert_process_support_failure(
            "refuse every submission while an abandoned",
            header,
            "\t\t\t\tif (Quarantined) {\n"
            "\t\t\t\t\treturn TTraits::failure(lane_failure::reap_failure);\n"
            "\t\t\t\t}\n"
            "\t\t\t\tBusy    = true;",
            "\t\t\t\tBusy    = true;",
        )
        self.assert_process_support_failure(
            "completion watchdog must predicate-check",
            header,
            "return aItem->Result.has_value();",
            "return true;",
        )
        self.assert_process_support_failure(
            "completion watchdog must predicate-check",
            header,
            "\t\t\t\t\tif (Busy) {\n"
            "\t\t\t\t\t\tif (!Quarantined) {\n"
            "\t\t\t\t\t\t\tobserve(*aItem, lane_observation::quarantined);\n"
            "\t\t\t\t\t\t}\n"
            "\t\t\t\t\t\tQuarantined = true;\n"
            "\t\t\t\t\t}\n",
            "",
        )
        self.assert_process_support_failure(
            "exception recovery must clear",
            header,
            "\t\t\t\t\t\tQuarantined = false;\n"
            "\t\t\t\t\t\tBusy        = false;\n",
            "\t\t\t\t\t\tBusy        = false;\n",
        )
        forbidden = "Forbidden launch, environment"
        # The environment and Windows job surfaces are refused earlier still,
        # by the effect-owner lists this target is not on.
        for hostile, expected in (
            ("\nvoid launch(QProcess& aProcess) { aProcess.start(); }\n", forbidden),
            ("\nvoid name(QProcess& aProcess) { aProcess.setProgram({}); }\n", forbidden),
            ("\nauto environment = QProcessEnvironment{};\n", "QProcessEnvironment ownership violation"),
            ("\nvoid detach(std::thread& aWorker) { aWorker.detach(); }\n", forbidden),
            ("\nauto job = CreateJobObjectW(nullptr, nullptr);\n", "containment API ownership violation"),
        ):
            with self.subTest(hostile_surface=hostile.strip()):
                self.assert_process_support_failure(
                    expected,
                    source,
                    "namespace bpr::adapters::process_support {\n",
                    hostile + "namespace bpr::adapters::process_support {\n",
                )
        self.assert_process_support_failure(
            "must not include a first-party layer",
            source,
            "#include <QObject>\n",
            "#include <QObject>\n#include \"bpr/platform/windows_standard_handles.hpp\"\n",
        )
        for dependency in (
            "BetterPullReview::Application",
            "BetterPullReview::AdapterGitHub",
            "Qt6::Network",
        ):
            with self.subTest(dependency=dependency):
                self.assert_process_support_failure(
                    "must depend on Qt Core only",
                    cmake,
                    "            Qt6::Core\n",
                    "            Qt6::Core\n            " + dependency + "\n",
                )
        self.assert_process_support_failure(
            "must be the static",
            cmake,
            "better_pull_review_process_support\n        STATIC",
            "better_pull_review_process_support\n        SHARED",
        )
        self.assert_failure(
            "through the shared serialized process lane",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "process_support::serialized_process_lane<"
                        "process_lane_traits> Lane;",
                        "process_lane_traits Lane;",
                    )
            },
        )
        self.assert_failure(
            "through the shared serialized process lane",
            {
                "src/adapters/git/src/installed_git_process.cpp":
                    GIT_PROCESS_RUNNER_SOURCE.replace(
                        '#include "bpr/adapters/process_support/process_supervision.hpp"\n',
                        "",
                    )
            },
        )
        self.assert_failure(
            "QProcess ownership violation",
            {
                "src/adapters/process_support/src/second_supervision.cpp":
                    "void f(QProcess&);\n",
            },
        )

    def test_installed_git_runner_requires_closed_hardening_contract(
        self,
    ) -> None:
        required_fragments = (
            "QProcess::SeparateChannels",
            "QProcess::ManagedInputChannel",
            "QProcess::UnixProcessFlag::CreateNewSession",
            "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE",
            "process_support::finish_background_reap",
            "empty_private_directory",
            "class private_input_file final",
            "O_NOFOLLOW",
            "FILE_FLAG_OPEN_REPARSE_POINT",
            "maximum_pending_input_bytes",
            "bufferedInputOffset",
            '"GIT_CONFIG_NOSYSTEM"',
            '"GIT_EXEC_PATH"',
            '"core.fsync=all"',
            '"core.fsyncMethod=fsync"',
            '"protocol.allow=never"',
            '"--check-self-contained-and-connected"',
            '"--max-input-size="',
            "probe_pack_sha256",
            "capability_probe(",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "Installed-Git runner contract is incomplete",
                    {
                        "src/adapters/git/src/"
                        "installed_git_process.cpp":
                            GIT_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "",
                            )
                    },
                )

    def test_installed_git_runner_restricts_unguarded_windows_launches(
        self,
    ) -> None:
        # [[BPR-1115]] Every unguarded Windows Git takes the platform target's
        # launch restriction, and a Git started without it is refused. Each
        # piece is required, so removing one on any host fails the gate even
        # though only Windows runs the row that exercises it.
        for fragment in (
            '#include "bpr/platform/windows_standard_handles.hpp"',
            "windows_standard_handles::launch_restriction",
            "Inheritance.apply(aArguments->startupInfo, aArguments->flags, "
            "aArguments->inheritHandles)",
            "restrict_to_standard_handles(*process",
            # [[BPR-1265]]. Git is created inside its job, and refused if
            # found outside it.
            "inheritance.place_in_job(job.native())",
            "!inheritance.placed_in_job()",
            "IsProcessInJob(",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, GIT_PROCESS_RUNNER_SOURCE)
                self.assert_failure(
                    "Installed-Git runner contract is incomplete",
                    {
                        "src/adapters/git/src/"
                        "installed_git_process.cpp":
                            GIT_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_STANDARD_HANDLE_RESTRICTION",
                            )
                    },
                )

    def test_platform_owns_the_shared_launch_restriction(self) -> None:
        # [[BPR-1115]] The platform header may build the handle list; the same
        # code anywhere else is refused, and the permission is the platform
        # target's own header.
        platform = {
            PLATFORM_STANDARD_HANDLES_HEADER: PLATFORM_STANDARD_HANDLES_SOURCE,
            "src/platform/CMakeLists.txt": PLATFORM_TARGET,
        }
        result = self.run_check(platform)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for other in (
            "src/adapters/github/src/windows_standard_handles.hpp",
            "src/adapters/native/windows_standard_handles.hpp",
        ):
            with self.subTest(other=other):
                self.assert_failure(
                    "native Windows agent containment API ownership violation",
                    {**platform, other: PLATFORM_STANDARD_HANDLES_SOURCE},
                )
        self.assert_failure(
            # CMake wraps its message before the owner's name.
            "Designated effect source windows_standard_handles.hpp",
            {
                PLATFORM_STANDARD_HANDLES_HEADER:
                    PLATFORM_STANDARD_HANDLES_SOURCE,
                "src/platform/CMakeLists.txt":
                    "add_library(better_pull_review_platform INTERFACE)\n",
            },
        )

    def test_installed_git_runner_rejects_expanded_authority(self) -> None:
        hostile_surfaces = (
            (
                "QNetworkReply* reply;\n",
                "Qt network/TLS type ownership violation",
            ),
            (
                "auto inherited = "
                "QProcessEnvironment::systemEnvironment();\n",
                "Forbidden installed-Git process surface",
            ),
            (
                'const char* verb = "fetch-pack";\n',
                "Forbidden installed-Git process surface",
            ),
            (
                "process.startCommand(command);\n",
                "Forbidden installed-Git process surface",
            ),
            (
                'const char* url = "https://example.invalid";\n',
                "Forbidden installed-Git process surface",
            ),
            (
                "#define BPR_GIT_TEST_SEAM 1\n",
                "Forbidden installed-Git process surface",
            ),
        )
        for hostile, expected in hostile_surfaces:
            with self.subTest(hostile=hostile.strip()):
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/git/src/"
                        "installed_git_process.cpp":
                            GIT_PROCESS_RUNNER_SOURCE + hostile
                    },
                )

    def test_git_application_port_rejects_generic_process_controls(
        self,
    ) -> None:
        self.assert_failure(
            "Git application port exposes forbidden process/provider "
            "authority",
            {
                "src/application/include/bpr/application/"
                "git_process_port.hpp":
                    GIT_PROCESS_PORT
                    + "\nstruct hostile { std::vector<std::string> "
                    "Arguments; };\n"
            },
        )

    def test_git_application_port_permits_one_bounded_diagnostic_string(
        self,
    ) -> None:
        # BPR-0348. The gate used to forbid `std::string` outright, which is why
        # a failing git subprocess could not explain itself. The ruling permits
        # exactly one diagnostic member. BPR-0409 now supplies that member on
        # the live port, so the unmodified fixture is the positive case.
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_git_application_port_rejects_a_second_diagnostic_member(
        self,
    ) -> None:
        self.assert_failure(
            "more than one diagnostic member",
            {
                "src/application/include/bpr/application/"
                "git_process_port.hpp":
                    GIT_PROCESS_PORT
                    + "\nstruct hostile { "
                    "std::string Detail; };\n"
            },
        )

    def test_git_application_port_rejects_text_outside_the_diagnostic_member(
        self,
    ) -> None:
        # Permitting text is not permitting arbitrary text: a second string
        # cannot arrive unnoticed. `Chatter` matches none of the named-authority
        # patterns, so this isolates the new rule rather than an older one.
        self.assert_failure(
            "carries text outside",
            {
                "src/application/include/bpr/application/"
                "git_process_port.hpp":
                    GIT_PROCESS_PORT
                    + "\nstruct hostile { std::string Chatter; };\n"
            },
        )

    def test_git_application_port_still_rejects_collections(self) -> None:
        # `std::string` and `std::vector` used to share one alternation. Removing
        # the string half without separating them would have admitted argument
        # and environment lists in the same edit. `Values` matches no
        # named-authority pattern, so only the collection rule can reject this.
        self.assert_failure(
            "Git application port exposes forbidden process/provider "
            "authority",
            {
                "src/application/include/bpr/application/"
                "git_process_port.hpp":
                    GIT_PROCESS_PORT
                    + "\nstruct hostile { std::vector<int> Values; };\n"
            },
        )

    def test_installed_git_target_rejects_test_only_seams(self) -> None:
        self.assert_failure(
            "must not compile a test-only production seam",
            git_target=GIT_TARGET
            + "\nif(BPR_BUILD_TESTS)\nendif()\n",
        )

    def test_quill_headers_are_owned_by_logging_adapter(self) -> None:
        self.assert_failure(
            "Quill effects belong exclusively to the logging adapter",
            {"src/app/src/unowned_logging.cpp": "#include <quill/Backend.h>\n"},
        )

    def test_core_application_and_qt_reject_process_and_network_types(
        self,
    ) -> None:
        for region in ("core", "application", "adapters/qt"):
            for effect in ("QProcess", "QNetworkReply", "QJsonDocument"):
                with self.subTest(region=region, effect=effect):
                    self.assert_failure(
                        "Core/application/Qt effect-boundary violation",
                        {
                            f"src/{region}/hostile.cpp":
                                f"{effect}* forbidden_effect;\n"
                        },
                    )

    def test_core_and_qt_reject_raw_provider_data(self) -> None:
        for region in ("core", "adapters/qt"):
            for provider_data in (
                'const char* endpoint = "https://api.example.test";\n',
                'const char* header = "Authorization: Bearer canary";\n',
            ):
                with self.subTest(
                    region=region,
                    provider_data=provider_data,
                ):
                    expected = (
                        "Core/application standard-C++ boundary violation"
                        if region == "core"
                        and "Authorization" in provider_data
                        else "Provider data crossed the core/Qt boundary"
                    )
                    self.assert_failure(
                        expected,
                        {f"src/{region}/hostile.cpp": provider_data},
                    )
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {"src/adapters/qt/hostile.cpp": "QUrl provider_url;\n"},
        )

    def test_application_rejects_provider_json_libraries(self) -> None:
        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {
                "src/application/hostile.cpp":
                    "#include <nlohmann/json.hpp>\n"
            },
        )

    def test_application_rejects_qt_value_types(self) -> None:
        self.assert_failure(
            "Core/application standard-C++ boundary violation",
            {
                "src/application/include/provider.hpp":
                    "#include <QString>\nQString provider_value;\n"
            },
        )

    def test_worker_executor_rejects_qt_and_provider_ownership(self) -> None:
        hostile_surfaces = (
            "QWidget* retained_widget;\n",
            "QAbstractItemModel* retained_model;\n",
            "github_provider* retained_provider;\n",
            "native_https_transport* retained_transport;\n",
            "validated_github_host_profile* retained_profile;\n",
            '#include "../../adapters/github/src/'
            'native_https_transport.hpp"\n',
        )
        for hostile_source in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                self.assert_failure(
                    "Worker executor Qt/provider boundary violation",
                    {
                        "src/app/src/worker_executor.cpp": hostile_source,
                    },
                )

    def test_core_and_application_reject_github_adapter_includes(self) -> None:
        for region in ("core", "application"):
            with self.subTest(region=region):
                self.assert_failure(
                    "Core/application standard-C++ boundary violation",
                    {
                        f"src/{region}/hostile.cpp":
                            '#include "../adapters/github/src/'
                            'github_read_contract.hpp"\n'
                    },
                )

    def test_core_application_and_qt_reject_raw_provider_literals(
        self,
    ) -> None:
        for region in ("core", "application", "adapters/qt"):
            with self.subTest(region=region):
                expected = (
                    "Core/application standard-C++ boundary violation"
                    if region != "adapters/qt"
                    else "Provider data crossed the core/Qt boundary"
                )
                self.assert_failure(
                    expected,
                    {
                        f"src/{region}/hostile.cpp":
                            'const char* raw = "{\\"node_id\\":'
                            '\\"provider-id\\"}";\n'
                    },
                )

    def test_application_rejects_embedded_provider_authority_and_auth(
        self,
    ) -> None:
        for literal in (
            'const char* endpoint = "https://api.github.com";\n',
            'const char* auth = "Authorization: Bearer canary";\n',
        ):
            with self.subTest(literal=literal):
                self.assert_failure(
                    "Core/application standard-C++ boundary violation",
                    {"src/application/hostile.cpp": literal},
                )

    def test_qt_rejects_normalized_provider_headers(self) -> None:
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {
                "src/adapters/qt/provider.cpp":
                    '#include "bpr/application/github_provider_port.hpp"\n'
            },
        )

    def test_core_rejects_application_provider_headers(self) -> None:
        self.assert_failure(
            "Provider data crossed the core/Qt boundary",
            {
                "src/core/provider.cpp":
                    '#include "bpr/application/github_read_model.hpp"\n'
            },
        )

    def test_qt_accepts_only_the_safe_github_projection_contract(
        self,
    ) -> None:
        result = self.run_check(
            {
                "src/adapters/qt/safe_projection.cpp": (
                    '#include "bpr/application/github_projection.hpp"\n'
                    "bpr::application::github_presentation_update* update;\n"
                    "bpr::application::github_intent_dispatch* dispatch;\n"
                )
            },
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_qt_rejects_other_application_github_contracts(self) -> None:
        forbidden_headers = (
            (
                "github_application_host.hpp",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_read_model.hpp",
                "Provider data crossed the core/Qt boundary",
            ),
            (
                "github_provider_port.hpp",
                "Provider data crossed the core/Qt boundary",
            ),
            (
                "github_credential_vault.hpp",
                "Credential-vault capability crossed the core/Qt boundary",
            ),
            (
                "github_capability_cache.hpp",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_host_authority.hpp",
                "Qt adapter GitHub projection boundary violation",
            ),
        )
        for header, expected_message in forbidden_headers:
            with self.subTest(header=header):
                self.assert_failure(
                    expected_message,
                    {
                        "src/adapters/qt/hostile.cpp": (
                            f'#include "bpr/application/{header}"\n'
                        )
                    },
                )

    def test_qt_rejects_github_adapter_headers_and_types(self) -> None:
        hostile_surfaces = (
            (
                '#include "../github/src/github_provider.hpp"\n',
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                '#include "bpr/adapters/github/github_provider.hpp"\n',
                "Provider data crossed the core/Qt boundary",
            ),
            (
                "bpr::adapters::github::github_provider* provider;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "validated_github_host_profile* profile;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "native_https_transport* transport;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_paged_read* pager;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
        )
        for surface, expected_message in hostile_surfaces:
            with self.subTest(surface=surface):
                self.assert_failure(
                    expected_message,
                    {"src/adapters/qt/hostile.cpp": surface},
                )

    def test_qt_rejects_github_effect_and_wire_value_types(self) -> None:
        hostile_surfaces = (
            (
                "QNetworkAccessManager* network;\n",
                "Core/application/Qt effect-boundary violation",
            ),
            (
                "QProcess* process;\n",
                "Core/application/Qt effect-boundary violation",
            ),
            (
                "QJsonDocument* document;\n",
                "Core/application/Qt effect-boundary violation",
            ),
            (
                "QUrl* url;\n",
                "Provider data crossed the core/Qt boundary",
            ),
        )
        for surface, expected_message in hostile_surfaces:
            with self.subTest(surface=surface):
                self.assert_failure(
                    expected_message,
                    {"src/adapters/qt/hostile.cpp": surface},
                )

    def test_qt_rejects_url_auth_credential_provider_and_transport_material(
        self,
    ) -> None:
        hostile_surfaces = (
            (
                'const char* endpoint = "https://api.example.test";\n',
                "Provider data crossed the core/Qt boundary",
            ),
            (
                'const char* authorization = '
                '"Authorization: Bearer canary";\n',
                "Provider data crossed the core/Qt boundary",
            ),
            (
                "github_credential_material material;\n",
                "Credential-vault capability crossed the core/Qt boundary",
            ),
            (
                "github_provider_request provider_request;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_account_transport account_transport;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_authentication_transport authentication_transport;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_capability_port* capability_port;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                'const char* endpoint = "api.github.com";\n',
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "Authorization* authorization;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_bootstrap_credential credential;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_pull_request_snapshot snapshot;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
            (
                "github_profile_authority authority;\n",
                "Qt adapter GitHub projection boundary violation",
            ),
        )
        for surface, expected_message in hostile_surfaces:
            with self.subTest(surface=surface):
                self.assert_failure(
                    expected_message,
                    {"src/adapters/qt/hostile.cpp": surface},
                )

    def test_core_and_qt_reject_credential_vault_contract_types(self) -> None:
        forbidden_surfaces = (
            '#include "bpr/application/github_credential_vault.hpp"\n',
            "github_credential_material material;\n",
            "github_credential_enrollment enrollment;\n",
            "github_credential_vault_port* vault;\n",
            "github_credential_retrieve_success* success;\n",
            "github_credential_retrieve_completion* completion;\n",
            "github_credential_retrieve_completion_handler* handler;\n",
        )
        for region in ("core", "adapters/qt"):
            for surface in forbidden_surfaces:
                with self.subTest(region=region, surface=surface):
                    self.assert_failure(
                        "Credential-vault capability crossed the "
                        "core/Qt boundary",
                        {f"src/{region}/hostile.cpp": surface},
                    )

    def test_application_and_application_tests_may_name_vault_contract(
        self,
    ) -> None:
        contract_use = (
            '#include "bpr/application/github_credential_vault.hpp"\n'
            "github_credential_material material;\n"
            "github_credential_enrollment enrollment;\n"
            "github_credential_vault_port* vault;\n"
        )
        result = self.run_check(
            {
                "src/application/include/contract.hpp": contract_use,
                "tests/application/contract_tests.cpp": contract_use,
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_application_target_cannot_link_qt_or_provider_adapter(self) -> None:
        self.assert_failure(
            "Application target must depend inward on Core only",
            {
                "src/application/CMakeLists.txt":
                    "target_link_libraries(app PRIVATE Qt6::Core)\n"
            },
        )

    def test_qt_target_cannot_link_provider_adapter(self) -> None:
        self.assert_failure(
            "Qt presentation adapter must not link the GitHub provider adapter",
            {
                "src/adapters/qt/CMakeLists.txt":
                    "target_link_libraries(qt PRIVATE "
                    "BetterPullReview::AdapterGitHub)\n"
            },
        )

    def test_designated_effect_source_must_belong_to_github_target(self) -> None:
        self.assert_failure(
            "GitHub host adapter source must be explicitly owned",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE
            },
            github_target=GITHUB_TARGET.replace(
                "        src/github_cli_process_runner.cpp\n",
                "",
            ),
        )

    def test_network_manager_is_rejected_outside_designated_transport(self) -> None:
        self.assert_failure(
            "QNetworkAccessManager ownership violation",
            {
                "src/adapters/github/src/other_transport.cpp":
                    "QNetworkAccessManager* manager;\n"
            },
        )

    def test_test_network_effect_is_owned_only_by_designated_live_parent(
        self,
    ) -> None:
        result = self.run_check(
            {
                "tests/network_isolation/"
                "designated_live_mutation_runner.cpp":
                    DESIGNATED_LIVE_TEST_NETWORK_RUNNER,
            }
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        self.assert_failure(
            "Test-only network effect ownership violation",
            {
                "tests/network_isolation/"
                "designated_live_mutation_runner.cpp":
                    DESIGNATED_LIVE_TEST_NETWORK_RUNNER,
                "tests/network_isolation/arbitrary_reader.cpp":
                    "QNetworkAccessManager* network;\n",
            },
        )

    def test_designated_live_parent_rejects_alternate_authority(self) -> None:
        for hostile in (
            'const char* other = "https://example.invalid";\n',
            'const char* header = "Authorization";\n',
            'auto value = qEnvironmentVariable("BPR_TARGET");\n',
            "manager.get(other_request);\n",
            "manager.post(other_request, body);\n",
        ):
            with self.subTest(hostile=hostile.strip()):
                expected = (
                    "exactly one GET call"
                    if "manager.get" in hostile
                    else "forbidden alternate network"
                )
                self.assert_failure(
                    expected,
                    {
                        "tests/network_isolation/"
                        "designated_live_mutation_runner.cpp":
                            DESIGNATED_LIVE_TEST_NETWORK_RUNNER + hostile,
                    },
                )

    def test_designated_live_canary_rejects_host_authority(self) -> None:
        exact_sources = {
            "tests/network_isolation/designated_live_mutation_canary.cpp":
                DESIGNATED_LIVE_TEST_CANARY,
            "tests/network_isolation/designated_live_mutation_canary.hpp":
                DESIGNATED_LIVE_TEST_CANARY_HEADER,
            "tests/network_isolation/designated_live_mutation_child.cpp":
                DESIGNATED_LIVE_TEST_CHILD,
            "tests/network_isolation/designated_live_mutation_child.hpp":
                DESIGNATED_LIVE_TEST_CHILD_HEADER,
            "tests/CMakeLists.txt": DESIGNATED_LIVE_TEST_CMAKE,
        }
        result = self.run_check(exact_sources)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        for hostile in (
            "std::system(command);\n",
            "popen(command, mode);\n",
            "QProcess process;\n",
            "fork();\n",
            "execve(path, argv, envp);\n",
            "posix_spawn(pid, path, actions, attr, argv, envp);\n",
            "socket(domain, type, protocol);\n",
            "curl_easy_init();\n",
            "write(fd, body, size);\n",
            'const char* url = "ht" "tps" "://example.invalid";\n',
        ):
            with self.subTest(hostile=hostile.strip()):
                mutated = dict(exact_sources)
                mutated[
                    "tests/network_isolation/"
                    "designated_live_mutation_canary.cpp"
                ] = DESIGNATED_LIVE_TEST_CANARY + hostile
                self.assert_failure(
                    "Designated live canary",
                    mutated,
                )

    def test_designated_live_canary_headers_are_transitively_locked(self) -> None:
        exact_sources = {
            "tests/network_isolation/designated_live_mutation_canary.cpp":
                DESIGNATED_LIVE_TEST_CANARY,
            "tests/network_isolation/designated_live_mutation_canary.hpp":
                DESIGNATED_LIVE_TEST_CANARY_HEADER,
            "tests/network_isolation/designated_live_mutation_child.cpp":
                DESIGNATED_LIVE_TEST_CHILD,
            "tests/network_isolation/designated_live_mutation_child.hpp":
                DESIGNATED_LIVE_TEST_CHILD_HEADER,
            "tests/CMakeLists.txt": DESIGNATED_LIVE_TEST_CMAKE,
        }
        hostile_headers = (
            "inline int hostile_initializer = [] { return 1; }();\n",
            "inline int hostile_system = system(command);\n",
            "inline QNetworkAccessManager hostile_network;\n",
            "inline std::vector<int> hostile_allocation;\n",
        )
        for header in (
            "designated_live_mutation_canary.hpp",
            "designated_live_mutation_child.hpp",
        ):
            for hostile in hostile_headers:
                with self.subTest(header=header, hostile=hostile.strip()):
                    mutated = dict(exact_sources)
                    path = f"tests/network_isolation/{header}"
                    mutated[path] = exact_sources[path] + hostile
                    expected = (
                        "Test-only network"
                        if "QNetworkAccessManager" in hostile
                        else "Designated live canary"
                    )
                    self.assert_failure(expected, mutated)

    def test_designated_live_fork_child_is_exact_and_single_authority(self) -> None:
        exact_sources = {
            "tests/network_isolation/"
            "designated_live_mutation_canary.cpp":
                DESIGNATED_LIVE_TEST_CANARY,
            "tests/network_isolation/"
            "designated_live_mutation_canary.hpp":
                DESIGNATED_LIVE_TEST_CANARY_HEADER,
            "tests/network_isolation/"
            "designated_live_mutation_child.cpp":
                DESIGNATED_LIVE_TEST_CHILD,
            "tests/network_isolation/"
            "designated_live_mutation_child.hpp":
                DESIGNATED_LIVE_TEST_CHILD_HEADER,
            "tests/CMakeLists.txt": DESIGNATED_LIVE_TEST_CMAKE,
        }
        result = self.run_check(exact_sources)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        for hostile in (
            "QProcess process;\n",
            "std::string allocated;\n",
            "malloc(64);\n",
            "system(command);\n",
            "execve(path, argv, envp);\n",
            "posix_spawn(pid, path, actions, attr, argv, envp);\n",
            "fork();\n",
            'const char* url = "ht" "tps" "://example.invalid";\n',
        ):
            with self.subTest(hostile=hostile.strip()):
                mutated = dict(exact_sources)
                mutated[
                    "tests/network_isolation/"
                    "designated_live_mutation_child.cpp"
                ] = DESIGNATED_LIVE_TEST_CHILD + hostile
                self.assert_failure("Designated live", mutated)

    def test_designated_live_runner_target_composition_is_exact(self) -> None:
        exact_sources = {
            "tests/network_isolation/designated_live_mutation_canary.cpp":
                DESIGNATED_LIVE_TEST_CANARY,
            "tests/network_isolation/designated_live_mutation_canary.hpp":
                DESIGNATED_LIVE_TEST_CANARY_HEADER,
            "tests/network_isolation/designated_live_mutation_child.cpp":
                DESIGNATED_LIVE_TEST_CHILD,
            "tests/network_isolation/designated_live_mutation_child.hpp":
                DESIGNATED_LIVE_TEST_CHILD_HEADER,
            "tests/CMakeLists.txt": DESIGNATED_LIVE_TEST_CMAKE,
        }
        mutations = (
            DESIGNATED_LIVE_TEST_CMAKE.replace(
                '        "${PROJECT_SOURCE_DIR}/tests/network_isolation/'
                'designated_live_mutation_runner.cpp"\n',
                '        "${PROJECT_SOURCE_DIR}/tests/network_isolation/'
                'designated_live_mutation_runner.cpp"\n'
                '        "${PROJECT_SOURCE_DIR}/tests/network_isolation/'
                'hostile.cpp"\n',
                1,
            ),
            DESIGNATED_LIVE_TEST_CMAKE
            + "\ntarget_sources("
            "tool_better_pull_review_designated_live_mutation_runner "
            "PRIVATE hostile.cpp)\n",
            DESIGNATED_LIVE_TEST_CMAKE.replace(
                "            Qt6::Network\n",
                "            Qt6::Network\n            Hostile::Library\n",
                1,
            ),
            DESIGNATED_LIVE_TEST_CMAKE.replace(
                "            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1\n",
                "            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1\n"
                "            HOSTILE_DEFINITION=1\n",
                1,
            ),
        )
        for mutated_cmake in mutations:
            with self.subTest(mutation=mutated_cmake[-120:]):
                mutated = dict(exact_sources)
                mutated["tests/CMakeLists.txt"] = mutated_cmake
                self.assert_failure(
                    "Designated live",
                    mutated,
                )

    def test_all_qt_network_and_tls_types_are_owned_by_native_https(self) -> None:
        for effect in ("QNetworkReply", "QSslCertificate"):
            with self.subTest(effect=effect):
                self.assert_failure(
                    "Qt network/TLS type ownership violation",
                    {
                        "src/adapters/github/src/other.cpp":
                            f"{effect}* forbidden_effect;\n"
                    },
                )

    def test_native_https_target_requires_private_qt_network_and_test_seam(
        self,
    ) -> None:
        mutations = (
            (
                "        Qt6::Network\n",
                "",
                "must link Qt6::Network privately",
            ),
            (
                "if(BUILD_TESTING)",
                "if(TRUE)",
                "fixture seams must compile privately",
            ),
            (
                "        PRIVATE\n            BPR_NATIVE_HTTPS_TESTING=1",
                "        PUBLIC\n            BPR_NATIVE_HTTPS_TESTING=1",
                "fixture seams must compile privately",
            ),
        )
        for original, replacement, expected in mutations:
            with self.subTest(expected=expected):
                self.assert_failure(
                    expected,
                    github_target=GITHUB_TARGET.replace(
                        original,
                        replacement,
                    ),
                )

    def test_native_https_headers_reject_qt_and_platform_effects(self) -> None:
        for hostile, expected in (
            (
                "#include <QNetworkReply>\n",
                "Qt network/TLS type ownership violation",
            ),
            (
                "#include <windows.h>\n",
                "native HTTPS interfaces must remain standard C++",
            ),
        ):
            with self.subTest(hostile=hostile.strip()):
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/native_https_transport.hpp":
                            NATIVE_HTTPS_TRANSPORT_INTERFACE + hostile,
                    },
                )

    def test_native_https_requires_system_trust_proxy_and_manual_policy(
        self,
    ) -> None:
        mutations = (
            "QNetworkProxyFactory::systemProxyForQuery();",
            "constexpr int maximum_system_proxy_candidates = 16;",
            "bool proxy_candidate_retryable() { return true; }",
            "QNetworkRequest::ManualRedirectPolicy",
            "QSslSocket::VerifyPeer",
            "QSsl::TlsV1_2OrLater",
            "    auto encrypted = &QNetworkReply::encrypted;",
            'request.setRawHeader("Accept-Encoding", "identity");',
            "std::stop_callback callback{stop, [] {}};",
            "struct active_hop_guard {};",
            "void remaining_before_deadline() {}",
            "    state.Done.notify_one();",
            "validate_github_redirect_target();",
        )
        for selected in mutations:
            with self.subTest(selected=selected):
                self.assert_failure(
                    "native HTTPS must retain explicit",
                    {
                        "src/adapters/github/src/native_https_transport.cpp":
                            NATIVE_HTTPS_TRANSPORT_SOURCE.replace(
                                selected,
                                "",
                            ),
                    },
                )

    def test_native_https_rejects_bypass_global_and_unbounded_surfaces(
        self,
    ) -> None:
        hostile_surfaces = (
            ("reply.ignoreSslErrors();\n", "certificate bypass"),
            (
                "QSslConfiguration::setDefaultConfiguration(tls);\n",
                "process-global TLS mutation",
            ),
            (
                "QNetworkProxy::setApplicationProxy({});\n",
                "global proxy mutation",
            ),
            ("reply.readAll();\n", "unbounded response read"),
            ("qWarning() << reply.errorString();\n", "raw diagnostic"),
            ("QNetworkDiskCache cache;\n", "persistence"),
            ("auto value = qgetenv(\"HTTPS_PROXY\");\n", "ambient"),
        )
        for hostile, expected in hostile_surfaces:
            with self.subTest(expected=expected):
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/native_https_transport.cpp":
                            NATIVE_HTTPS_TRANSPORT_SOURCE + hostile,
                    },
                )

    def test_both_transports_require_shared_canonical_wire_validation(
        self,
    ) -> None:
        self.assert_failure(
            "transports must use the shared correlation",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    GH_CLI_TRANSPORT_SOURCE.replace(
                        '#include "github_wire_request_validation.hpp"\n',
                        "",
                    ),
            },
        )
        self.assert_failure(
            "must recompile and validate the canonical",
            {
                "src/adapters/github/src/github_wire_request_validation.cpp":
                    GITHUB_WIRE_VALIDATION_SOURCE.replace(
                        "validate_outbound_read_request();",
                        "std::expected<void, int>{};",
                    ),
            },
        )

    def test_qt_adapter_cannot_own_provider_effects(self) -> None:
        self.assert_failure(
            "Core/application/Qt effect-boundary violation",
            {"src/adapters/qt/provider.cpp": "QProcess* process;\n"},
        )

    def test_telemetry_hardening_is_rejected_outside_cli_transport(self) -> None:
        self.assert_failure(
            "GitHub CLI telemetry hardening ownership violation",
            {
                "src/application/src/provider.cpp":
                    "const char* setting = \"GH_TELEMETRY=0\";\n"
            },
        )

    def test_non_disabling_telemetry_setting_is_rejected_in_transport(
        self,
    ) -> None:
        self.assert_failure(
            "Forbidden telemetry surface",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    GH_CLI_TRANSPORT_SOURCE.replace(
                        "GH_TELEMETRY=0",
                        "GH_TELEMETRY=1",
                    )
            },
        )

    def test_cli_transport_source_requires_telemetry_hardening(self) -> None:
        self.assert_failure(
            "must set GH_TELEMETRY=0",
            {
                "src/adapters/github/src/gh_cli_transport.cpp":
                    GH_CLI_TRANSPORT_SOURCE.replace(
                        '    "GH_TELEMETRY=0",\n',
                        "",
                    )
            },
        )

    def test_github_host_designated_files_are_required(self) -> None:
        omissions = (
            {"omit_process_interface": True},
            {"omit_process_runner_interface": True},
            {"omit_process_runner": True},
            {"omit_cli_transport": True},
        )
        for omission in omissions:
            with self.subTest(omission=omission):
                self.assert_failure(
                    "designated GitHub host adapter file is required",
                    **omission,
                )

    def test_github_host_effect_sources_require_explicit_target_ownership(
        self,
    ) -> None:
        for source in (
            "        src/github_cli_process_runner.cpp\n",
            "        src/gh_cli_transport.cpp\n",
        ):
            with self.subTest(source=source):
                self.assert_failure(
                    "GitHub host adapter source must be explicitly owned",
                    github_target=GITHUB_TARGET.replace(source, ""),
                )

        thread_dependency_mutations = (
            (
                "        Threads::Threads\n",
                "",
                "must link Threads::Threads explicitly and privately",
            ),
            (
                "    PRIVATE\n        Threads::Threads\n",
                "    PUBLIC\n        Threads::Threads\n",
                "must link Threads::Threads explicitly and privately",
            ),
        )
        for original, mutation, expected in thread_dependency_mutations:
            with self.subTest(thread_dependency=original.strip()):
                self.assert_failure(
                    expected,
                    github_target=GITHUB_TARGET.replace(
                        original,
                        mutation,
                    ),
                )

    def test_cli_process_interfaces_reject_qt_and_os_process_types(
        self,
    ) -> None:
        hostile_headers = (
            (
                "src/adapters/github/src/github_cli_process.hpp",
                GITHUB_CLI_PROCESS_INTERFACE
                + "#include <QProcess>\nQProcess* process;\n",
            ),
            (
                "src/adapters/github/src/github_cli_process_runner.hpp",
                GITHUB_CLI_PROCESS_RUNNER_INTERFACE
                + "#include <QByteArray>\nQByteArray bytes;\n",
            ),
            (
                "src/adapters/github/src/github_cli_process.hpp",
                GITHUB_CLI_PROCESS_INTERFACE
                + "#include <sys/wait.h>\npid_t process_id;\n",
            ),
            (
                "src/adapters/github/src/github_cli_process.hpp",
                GITHUB_CLI_PROCESS_INTERFACE
                + "#include <windows.h>\nHANDLE process_handle;\n",
            ),
            (
                "src/adapters/github/src/github_cli_process.hpp",
                GITHUB_CLI_PROCESS_INTERFACE
                + "std::string StandardError;\n",
            ),
            (
                "src/adapters/github/src/github_cli_process.hpp",
                GITHUB_CLI_PROCESS_INTERFACE
                + "const char* forbidden = \"GH_TOKEN\";\n",
            ),
        )
        for path, source in hostile_headers:
            with self.subTest(path=path, source=source[-40:]):
                expected = (
                    "QProcess ownership violation"
                    if "QProcess" in source
                    else "process interface must remain standard C++"
                )
                self.assert_failure(
                    expected,
                    {path: source},
                )

        self.assert_failure(
            "production factory must remain nullary",
            {
                "src/adapters/github/src/"
                "github_cli_process_runner.hpp":
                    GITHUB_CLI_PROCESS_RUNNER_INTERFACE.replace(
                        "std::shared_ptr<"
                        "github_cli_process_runner_test_seam> test_seam",
                        "github_cli_process_runner_test_seam* test_seam",
                    )
            },
        )
        self.assert_failure(
            "production factory must remain nullary",
            {
                "src/adapters/github/src/"
                "github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "std::shared_ptr<"
                        "github_cli_process_runner_test_seam> TestSeam",
                        "github_cli_process_runner_test_seam* TestSeam",
                    )
            },
        )
        self.assert_failure(
            "production factory must remain nullary",
            {
                "src/adapters/github/src/"
                "github_cli_process_runner.hpp":
                    GITHUB_CLI_PROCESS_RUNNER_INTERFACE.replace(
                        "make_github_cli_process_runner()",
                        "make_github_cli_process_runner("
                        "std::shared_ptr<"
                        "github_cli_process_runner_test_seam>)",
                    )
            },
        )
        # BPR-0807: file limits reach a runner through the Git-acquisition
        # factory, which takes nothing else and never passes a seam.
        for path, fixture, hostile in (
            (
                "github_cli_process_runner.hpp",
                GITHUB_CLI_PROCESS_RUNNER_INTERFACE,
                (
                    "github_cli_process_file_limits aFileLimits)",
                    "github_cli_process_file_limits aFileLimits, "
                    "std::shared_ptr<github_cli_process_runner_test_seam> "
                    "seam)",
                ),
            ),
            (
                "github_cli_process_runner.hpp",
                GITHUB_CLI_PROCESS_RUNNER_INTERFACE,
                (
                    "auto make_github_cli_process_runner()",
                    "auto make_github_cli_process_runner("
                    "github_cli_process_file_limits aFileLimits = { })",
                ),
            ),
            (
                "github_cli_process_runner.cpp",
                GITHUB_CLI_PROCESS_RUNNER_SOURCE,
                (
                    "(nullptr, std::move(aFileLimits))",
                    "(GlobalSeam, std::move(aFileLimits))",
                ),
            ),
            (
                "github_cli_process_runner.cpp",
                GITHUB_CLI_PROCESS_RUNNER_SOURCE,
                (
                    "    return make_qprocess_github_cli_process_runner("
                    "nullptr, std::move(aFileLimits));\n",
                    "    return make_qprocess_github_cli_process_runner"
                    " (GlobalSeam);\n"
                    "    return make_qprocess_github_cli_process_runner("
                    "nullptr, std::move(aFileLimits));\n",
                ),
            ),
        ):
            with self.subTest(path=path, hostile=hostile[1]):
                self.assertIn(hostile[0], fixture)
                self.assert_failure(
                    "production factory must remain nullary",
                    {
                        "src/adapters/github/src/" + path:
                            fixture.replace(*hostile)
                    },
                )

    def test_cli_git_acquisition_factory_refusals_are_code_in_its_body(
        self,
    ) -> None:
        # BPR-0807: the Git-acquisition factory's refusals count only as
        # compiled code in its own body. A comment, a dead preprocessor
        # branch, an uncalled function or a deleted guard is refused.
        runner = "src/adapters/github/src/github_cli_process_runner.cpp"
        spool_guard = (
            "    if (!aFileLimits.StandardOutputFile.empty()\n"
            "        && (aFileLimits.StandardOutputFile.parent_path()"
            " != aFileLimits.Directory\n"
            "            || std::filesystem::exists("
            "aFileLimits.StandardOutputFile))) {\n"
            "        return nullptr;\n"
            "    }\n"
        )
        limit_guard = (
            "    if (aFileLimits.MaximumFileBytes == 0U"
            " || aFileLimits.MaximumFileBytes > 32U * 1024U * 1024U\n"
            "        || !aFileLimits.Directory.is_absolute()"
            " || aFileLimits.MaximumDirectoryBytes == 0U\n"
            "        || aFileLimits.MaximumDirectoryEntries == 0U"
            " || aFileLimits.MaximumDirectoryEntries > 256U) {\n"
            "        return nullptr;\n"
            "    }\n"
        )
        guards = spool_guard + limit_guard
        source = GITHUB_CLI_PROCESS_RUNNER_SOURCE
        self.assertIn(guards, source)
        commented = "".join(
            "    // " + line.strip() + "\n" for line in guards.splitlines()
        )
        uncalled = (
            "auto unused_guards(github_cli_process_file_limits aFileLimits)\n"
            "    -> std::unique_ptr<github_cli_process_port> {\n"
            + guards
            + "    return nullptr;\n"
            "}\n"
        )
        for name, hostile, fragment in (
            ("guards in line comments", source.replace(guards, commented), guards),
            (
                "guards in a block comment",
                source.replace(guards, "    /*\n" + guards + "    */\n"),
                guards,
            ),
            (
                "guards under #if 0",
                source.replace(guards, "#if 0\n" + guards + "#endif\n"),
                guards,
            ),
            (
                "guards in an uncalled function",
                source.replace(guards, "") + uncalled,
                guards,
            ),
            (
                "zero-limit refusal deleted",
                source.replace("aFileLimits.MaximumFileBytes == 0U || ", ""),
                "aFileLimits.MaximumFileBytes == 0U || ",
            ),
            (
                "relative-store refusal deleted",
                source.replace("|| !aFileLimits.Directory.is_absolute() ", ""),
                "|| !aFileLimits.Directory.is_absolute() ",
            ),
            (
                "existing-spool refusal deleted",
                source.replace(
                    "\n            || std::filesystem::exists("
                    "aFileLimits.StandardOutputFile)",
                    "",
                ),
                "std::filesystem::exists(aFileLimits.StandardOutputFile)",
            ),
        ):
            with self.subTest(factory=name):
                self.assertIn(fragment, source)
                self.assertNotEqual(hostile, source)
                self.assert_failure(
                    "Git-acquisition factory must refuse",
                    {runner: hostile},
                )

    def test_cli_runner_requires_explicit_bounded_process_lifecycle(
        self,
    ) -> None:
        required_fragments = (
            "#include <QProcess>\n",
            "#include <QProcessEnvironment>\n",
            "process->setProgram",
            "process->setArguments",
            "process->setProcessEnvironment",
            "process->setWorkingDirectory",
            'neutral_repository_marker = ".git"',
            'equal_environment_names(entry.Name, "PATH")',
            'equal_environment_names(entry.Name, "SystemRoot")',
            "QProcess::SeparateChannels",
            "QProcess::ManagedInputChannel",
            "process->write",
            "process->closeWriteChannel",
            "process->waitForStarted",
            "process->waitForReadyRead",
            "std::make_unique<QProcess>()",
            "request.StandardInputLimit",
            "request.StandardOutputLimit",
            "request.StandardErrorLimit",
            "request.Timeout",
            "request.ReapTimeout",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                expected = (
                    "pre-copy request-shape validation must retain all"
                    if fragment
                    in {
                        "request.StandardInputLimit",
                        "request.StandardOutputLimit",
                        "request.StandardErrorLimit",
                        "request.Timeout",
                        "request.ReapTimeout",
                    }
                    else "GitHub CLI process runner must retain explicit"
                )
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_REQUIRED_RUNNER_LIFECYCLE",
                            )
                    },
                )

        request_shape_fragments = (
            "request.Executable.size()",
            "request.WorkingDirectory.size()",
            "request.Arguments.size()",
            "request.Environment.size()",
            "request.StandardInput.size()",
            "request.StandardOutputLimit",
            "request.StandardErrorLimit",
            "request.Timeout",
            "request.ReapTimeout",
            "argument.empty()",
            "argument.size() > github_cli_process_max_argument_bytes",
            "contains_nul(argument)",
            "argument_bytes",
            "entry.Name.size()",
            "entry.Value.size()",
            "environment_bytes",
            "request.Environment[prior].Name",
        )
        for fragment in request_shape_fragments:
            with self.subTest(pre_copy_shape=fragment):
                self.assert_failure(
                    "pre-copy request-shape validation must retain all",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_PRE_COPY_SHAPE_BOUND",
                            )
                    },
                )

        for allocating_shape_surface in (
            "\n    std::string copied_path = request.Executable;\n",
            "\n    auto storage = std::make_unique<int>(1);\n",
            "\n    throw invalid_request{};\n",
        ):
            with self.subTest(
                allocating_shape_surface=allocating_shape_surface.strip(),
            ):
                mutated_shape = GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                    "    return true;\n}\n\n"
                    "[[nodiscard]] auto decode_utf8(",
                    allocating_shape_surface
                    + "    return true;\n}\n\n"
                    "[[nodiscard]] auto decode_utf8(",
                )
                self.assert_failure(
                    "pre-copy request-shape validation must remain",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp": mutated_shape
                    },
                )

        self.assert_failure(
            "pre-copy request-shape validation must remain",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "    return value.find('\\0') "
                        "!= std::string_view::npos;\n",
                        "    std::string copied{value};\n"
                        "    return copied.find('\\0') "
                        "!= std::string::npos;\n",
                    )
            },
        )

        helper_allocation_mutations = (
            (
                "    return !name.empty();\n",
                "    std::string copied{name};\n"
                "    return !copied.empty();\n",
            ),
            (
                "    return left == right;\n",
                "    throw invalid_request{};\n"
                "    return left == right;\n",
            ),
            (
                "    if(total > limit || amount > limit - total) {\n",
                "    auto storage = std::make_unique<int>(1);\n"
                "    if(total > limit || amount > limit - total) {\n",
            ),
        )
        for original, mutation in helper_allocation_mutations:
            with self.subTest(pre_copy_helper=original.strip()):
                self.assert_failure(
                    "pre-copy request-shape validation must remain",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                original,
                                mutation,
                            )
                    },
                )

        self.assert_failure(
            "aggregate pre-copy bounds must reject overflow",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "if(total > limit || amount > limit - total)",
                        "if(amount > limit - total)",
                    )
            },
        )

        shape_guard = """\
        if(!valid_request_shape(request)) {
            return std::unexpected{
                github_cli_process_failure::invalid_request};
        }
"""
        stop_guard = """\
        if(stop.stop_requested()) {
            return std::unexpected{
                github_cli_process_failure::cancelled};
        }
"""
        for guard in (shape_guard, stop_guard):
            with self.subTest(removed_pre_copy_guard=guard.splitlines()[0]):
                self.assert_failure(
                    "must validate request shape and cancellation",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                guard,
                                "",
                            )
                    },
                )

            reordered = GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                guard,
                "",
            ).replace(
                "            item->Request = request;\n",
                "            item->Request = request;\n" + guard,
            )
            with self.subTest(reordered_pre_copy_guard=guard.splitlines()[0]):
                self.assert_failure(
                    "must validate request shape and cancellation",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp": reordered
                    },
                )

        self.assert_failure(
            "exactly one guarded request-copy site",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE
                    + "\nvoid duplicate_copy() { item->Request = request; }\n"
            },
        )

        windows_environment_fragments = (
            "bool has_empty_path = false;",
            "bool has_system_root = false;",
            "has_empty_path = entry.Value.empty();",
            "has_system_root = !entry.Value.empty();",
            "if(!has_empty_path || !has_system_root)",
        )
        for fragment in windows_environment_fragments:
            with self.subTest(windows_environment=fragment):
                expected = (
                    "pre-copy request-shape validation must retain all"
                    if fragment == "if(!has_empty_path || !has_system_root)"
                    else "must retain explicit Windows empty PATH"
                )
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_WINDOWS_ENVIRONMENT_VALIDATION",
                            )
                    },
                )

        unix_containment_fragments = (
            "process->setUnixProcessParameters(",
            "QProcess::UnixProcessFlag::CloseFileDescriptors",
            "QProcess::UnixProcessFlag::DisableCoreDumps",
            "QProcess::UnixProcessFlag::ResetSignalHandlers",
            "QProcess::UnixProcessFlag::CreateNewSession",
            "&QProcess::started",
            "process_pointer->processId();",
            "kill_process_group_and_reap(",
            "finish_background_reap(",
        )
        for fragment in unix_containment_fragments:
            with self.subTest(unix_containment=fragment):
                self.assert_failure(
                    "must retain Unix child hardening",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_UNIX_PROCESS_CONTAINMENT",
                            )
                    },
                )

        process_group_resolution_mutations = (
            (
                """\
    process->start();
    *process_group_id = resolve_process_group(
        *process,
        *process_group_id);
""",
                "    process->start();\n",
                "must resolve the initial process group",
            ),
            (
                """\
    for(;;) {
        *process_group_id = resolve_process_group(
            *process,
            *process_group_id);
        if(const auto failure = terminal_failure(); failure.has_value()) {
""",
                """\
    for(;;) {
        if(const auto failure = terminal_failure(); failure.has_value()) {
        *process_group_id = resolve_process_group(
            *process,
            *process_group_id);
""",
                "must resolve the initial process group",
            ),
            (
                """\
    const auto process_group_id =
        resolve_process_group(*process, known_process_group_id);
    if(
        kill_process_group_and_reap(
            *process,
            process_group_id,
            reap_timeout)) {
""",
                """\
    const auto process_group_id = known_process_group_id;
    if(
        kill_process_group_and_reap(
            *process,
            process_group_id,
            reap_timeout)) {
""",
                "failed execution must resolve initial process-group",
            ),
        )
        for original, mutation, expected in process_group_resolution_mutations:
            with self.subTest(process_group_resolution=expected):
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                original,
                                mutation,
                            )
                    },
                )

        finished_connection = """\
    QObject::connect(
        process.get(),
        &QProcess::finished,
        process.get(),
        [process_group_id](
            const int,
            const QProcess::ExitStatus) noexcept {
            kill_process_group(*process_group_id);
        },
        Qt::DirectConnection);
"""
        self.assert_failure(
            "must capture stable process-group state",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        finished_connection,
                        "",
                    )
            },
        )
        self.assert_failure(
            "must capture stable process-group state",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        finished_connection,
                        finished_connection.replace(
                            "Qt::DirectConnection",
                            "Qt::QueuedConnection",
                        ),
                    )
            },
        )
        self.assert_failure(
            "finished process-group cleanup must be connected before",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        finished_connection,
                        "",
                    ).replace(
                        "    process->write(\n",
                        finished_connection + "    process->write(\n",
                    )
            },
        )

        self.assert_failure(
            "must capture stable process-group state",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "[process_group_id](",
                        "[&](",
                    )
            },
        )

        self.assert_failure(
            "must capture stable process-group state",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        """\
        [
            process_pointer = process.get(),
            process_group_id
        ]() noexcept {
""",
                        "        [&]() noexcept {\n",
                    )
            },
        )

        terminal_success_cleanup = """\
    kill_process_group(*process_group_id);
    return {
        .Result = github_cli_process_success{},
"""
        self.assert_failure(
            "must repeat initial process-group cleanup",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        terminal_success_cleanup,
                        """\
    return {
        .Result = github_cli_process_success{},
""",
                    )
            },
        )

        worker_fragments = (
            "class process_worker final",
            "Lane.submit(item, request.ReapTimeout)",
            "item->Deadline = clock::now() + request.Timeout;",
            "std::unique_ptr<process_worker> Worker;",
            "Worker = std::make_unique<process_worker>(",
            "std::mutex WorkerMutex;",
        )
        for fragment in worker_fragments:
            with self.subTest(worker_lifecycle=fragment):
                self.assert_failure(
                    "must retain its runner-owned",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_RUNNER_OWNED_WORKER_LIFECYCLE",
                            )
                    },
                )

        for hostile_worker in (
            "\nvoid detach_worker() { Worker.detach(); }\n",
            "\nvoid leak_worker() { Worker.release(); }\n",
            "\nauto* leaked_worker = new process_worker();\n",
            "\nprocess_worker global_worker;\n",
        ):
            with self.subTest(hostile_worker=hostile_worker):
                self.assert_failure(
                    "Forbidden global, detached, or leaked process worker",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_worker
                    },
                )

        self.assert_failure(
            "process worker must have exactly one owner",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE
                    + "\nstd::unique_ptr<process_worker> GlobalWorker;\n"
            },
        )
        for hostile_owner in (
            "\nauto GlobalWorker = std::make_unique<process_worker>();\n",
            "\nstd::thread GlobalWorkerThread;\n",
        ):
            with self.subTest(hostile_owner=hostile_owner):
                self.assert_failure(
                    "process worker must have exactly one owner",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_owner
                    },
                )

        for hostile_worker_view in (
            "\nprocess_worker* GlobalWorkerView;\n",
            "\nstd::shared_ptr<process_worker> SharedWorker;\n",
        ):
            with self.subTest(hostile_worker_view=hostile_worker_view):
                self.assert_failure(
                    "Forbidden unowned or shared process worker lifetime",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_worker_view
                    },
                )

        self.assert_failure(
            "must check shutdown, cancellation",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "|| stop.stop_requested())",
                        "|| false)",
                    )
            },
        )
        self.assert_failure(
            "must check shutdown, cancellation",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "if(clock::now() >= deadline)",
                        "if(false)",
                    )
            },
        )
        self.assert_failure(
            "exactly one guarded process start site",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        finished_connection,
                        finished_connection + "    process->start();\n",
                    )
            },
        )

        reap_ownership_fragments = (
            "std::unique_ptr<QProcess> AwaitingReap;",
            ".AwaitingReap = std::move(process),",
        )
        for fragment in reap_ownership_fragments:
            with self.subTest(reap_ownership=fragment):
                self.assert_failure(
                    "must retain quarantine ownership",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                fragment,
                                "REMOVED_REAP_QUARANTINE_OWNERSHIP",
                            )
                    },
                )

        self.assert_failure(
            "background reap must be the shared bounded reap",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                        "github_cli_process_max_reap_timeout,\n"
                        "            tree_kill_for(TestSeam));",
                        "std::chrono::hours{24},\n"
                        "            tree_kill_for(TestSeam));",
                    )
            },
        )

        self.assert_failure(
            "exactly the bounded-reap",
            {
                "src/adapters/github/src/github_cli_process_runner.cpp":
                    GITHUB_CLI_PROCESS_RUNNER_SOURCE
                    + "\nauto extra_reap_failure() { return "
                    "github_cli_process_failure::reap_failure; }\n"
            },
        )

        bounded_read_mutations = (
            (
                "std::array<char, 16U * 1024U> buffer{};",
                "std::vector<char> buffer(limit);",
            ),
            (
                "const auto remaining = limit - consumed;",
                "const auto remaining = limit;",
            ),
            (
                "remaining + 1U",
                "buffer.size()",
            ),
            (
                "process.read(\n"
                "            buffer.data(),\n"
                "            static_cast<qint64>(requested))",
                "process.read(buffer.data(), 1)",
            ),
            (
                "if(byte_count > remaining)",
                "if(false)",
            ),
            (
                "consumed += byte_count;",
                "consumed = byte_count;",
            ),
        )
        for original, mutation in bounded_read_mutations:
            with self.subTest(bounded_read=original):
                self.assert_failure(
                    "must retain bounded fixed-buffer",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                                original,
                                mutation,
                            )
                    },
                )

        for premature_release in (
            "\nvoid release_process() { process.reset(); }\n",
            "\nvoid delete_process() { delete process; }\n",
            "\nvoid release_reap() { AwaitingReap.release(); }\n",
            "\nvoid stack_process() { QProcess process; }\n",
            "\nvoid raw_process() { auto* process = new QProcess; }\n",
        ):
            with self.subTest(premature_release=premature_release):
                self.assert_failure(
                    "Forbidden premature process ownership release",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + premature_release
                    },
                )

    def test_cli_runner_rejects_shell_and_raw_command_surfaces(self) -> None:
        hostile_surfaces = (
            "process.startCommand(command);\n",
            "QProcess::splitCommand(command);\n",
            "process.setNativeArguments(command);\n",
            "process.startDetached();\n",
            "QProcess::execute(command);\n",
            "process.setCreateProcessArgumentsModifier(modifier);\n",
            'const char* shell = "/bin/sh";\n',
            'const char* shell = "cmd.exe";\n',
        )
        for hostile_surface in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    (
                        "Alternate process or shell API"
                        if "bin/sh" in hostile_surface
                        or "cmd.exe" in hostile_surface
                        else "Forbidden shell/raw-command"
                    ),
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

    def test_strict_cli_credential_exception_is_closed(self) -> None:
        runner = "src/adapters/github/src/github_cli_process_runner.cpp"
        source = (REPOSITORY_ROOT / runner).read_text(encoding="utf-8")
        result = self.run_check({runner: source})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_failure("Forbidden credential, status-output, cache, or debug", {
            runner: source + '\nconst char* arbitrary = "GH_TOKEN";\n'})
        for fragment in ("equal_environment_names(entry.Name, aName)",
                         "item->Credential = std::move(aCredential)",
                         "capture->finish_credential()"):
            with self.subTest(fragment=fragment):
                pattern = re.escape(fragment).replace(r"\ ", r"\s+")
                mutated, count = re.subn(pattern, "removed_sensitive_boundary", source)
                self.assertEqual(count, 1)
                self.assert_failure("GitHub CLI sensitive ownership contract missing", {runner: mutated})

    def test_cli_runner_restricts_every_windows_launch(self) -> None:
        # [[BPR-1114]] The runner's one process-creation modifier is the
        # platform launch restriction, pinned whole. Removing a piece, doubling
        # it, widening its body or leaving it only in a comment fails the gate;
        # a second modifier of any other shape is a raw-command surface.
        runner = "src/adapters/github/src/github_cli_process_runner.cpp"
        restriction = (
            "process->setCreateProcessArgumentsModifier([&inheritance, "
            "aFailStandardHandleList](QProcess::CreateProcessArguments* const "
            "aArguments) {\n"
            "        if (aFailStandardHandleList) {\n"
            "            aArguments->startupInfo->hStdError = nullptr;\n"
            "        }\n"
            "        inheritance.apply(aArguments->startupInfo, "
            "aArguments->flags, aArguments->inheritHandles);\n"
            "    });\n"
        )
        self.assertEqual(GITHUB_CLI_PROCESS_RUNNER_SOURCE.count(restriction), 1)
        missing = "must restrict every Windows launch to its"
        mutations = (
            ("removed", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                restriction, "")),
            ("doubled", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                restriction, restriction + "    " + restriction)),
            ("comment only", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                restriction, "/* " + restriction + " */\n")),
            ("unconditional withholding", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                "        if (aFailStandardHandleList) {\n"
                "            aArguments->startupInfo->hStdError = nullptr;\n"
                "        }\n",
                "        aArguments->startupInfo->hStdError = nullptr;\n")),
            ("program replaced", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                "        inheritance.apply(",
                "        aArguments->applicationName = nullptr;\n"
                "        inheritance.apply(")),
            ("inheritance forced", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                "aArguments->flags, aArguments->inheritHandles);",
                "aArguments->flags, aArguments->inheritHandles);\n"
                "        aArguments->inheritHandles = true;")),
            ("no restriction object", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                "bpr::platform::windows_standard_handles::launch_restriction "
                "inheritance;",
                "")),
            ("no post-start refusal", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                "    if (!inheritance.restricted()) {\n"
                "        return failed(github_cli_process_failure::start_failure);\n"
                "    }\n",
                "")),
            ("no platform header", GITHUB_CLI_PROCESS_RUNNER_SOURCE.replace(
                '#include "bpr/platform/windows_standard_handles.hpp"\n', "")),
        )
        for name, mutated in mutations:
            with self.subTest(mutation=name):
                self.assertNotEqual(mutated, GITHUB_CLI_PROCESS_RUNNER_SOURCE)
                self.assert_failure(missing, {runner: mutated})
        self.assert_failure(
            "Forbidden shell/raw-command surface",
            {
                runner: GITHUB_CLI_PROCESS_RUNNER_SOURCE
                + "void other(QProcess& process) {\n"
                "    process.setCreateProcessArgumentsModifier(\n"
                "        [](QProcess::CreateProcessArguments* a) {\n"
                "            a->applicationName = nullptr;\n"
                "        });\n"
                "}\n"
            },
        )

    def test_cli_runner_rejects_unsafe_channel_routing(self) -> None:
        hostile_surfaces = (
            (
                "process.setProcessChannelMode("
                "QProcess::ForwardedChannels);\n",
                "Forbidden forwarded, merged, or file-redirected",
            ),
            (
                "process.setProcessChannelMode(QProcess::MergedChannels);\n",
                "Forbidden forwarded, merged, or file-redirected",
            ),
            (
                "process.setReadChannel(QProcess::StandardError);\n",
                "Forbidden unselected process channel selection",
            ),
            (
                "process.setStandardOutputProcess(other_process);\n",
                "Forbidden forwarded, merged, or file-redirected",
            ),
            (
                'process.setStandardOutputFile("output.log");\n',
                "Forbidden forwarded, merged, or file-redirected",
            ),
            (
                'process.setStandardErrorFile("error.log");\n',
                "Forbidden forwarded, merged, or file-redirected",
            ),
            (
                'process.setStandardInputFile("input.txt");\n',
                "Forbidden forwarded, merged, or file-redirected",
            ),
        )
        for hostile_surface, expected in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

        # BPR-0807: RFC-BPR-0015 admits one guarded stdout spool, and only
        # where the same compiled span gives the child RLIMIT_FSIZE.
        spool = (
            "void configure_channels(std::unique_ptr<QProcess>& process,\n"
            "    const github_cli_process_file_limits& aFileLimits) {\n"
            "    if (!aFileLimits.StandardOutputFile.empty()) {\n"
            "        process->setStandardOutputFile("
            "QString::fromStdString(aFileLimits.StandardOutputFile.string()), "
            "QIODevice::Truncate);\n"
            "    }\n"
            "#if defined(Q_OS_UNIX)\n"
            "    process->setChildProcessModifier("
            "[limit = aFileLimits.MaximumFileBytes, child = process.get()] {\n"
            "        if (limit != 0U) {\n"
            "            const rlimit fileLimit{ static_cast<rlim_t>(limit), "
            "static_cast<rlim_t>(limit) };\n"
            "            if (::setrlimit(RLIMIT_FSIZE, &fileLimit) != 0) {\n"
            "                child->failChildProcessModifier("
            "\"file size limit\", errno);\n"
            "            }\n"
            "        }\n"
            "        bpr::platform::unix_descriptor_sweep::bound();\n"
            "    });\n"
            "#endif\n"
            "}\n"
        )
        limit = (
            "            if (::setrlimit(RLIMIT_FSIZE, &fileLimit) != 0) {\n"
            "                child->failChildProcessModifier("
            "\"file size limit\", errno);\n"
            "            }\n"
        )
        self.assertIn(limit, spool)
        runner = "src/adapters/github/src/github_cli_process_runner.cpp"
        result = self.run_check(
            {runner: GITHUB_CLI_PROCESS_RUNNER_SOURCE + spool}
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        commented_limit = "".join(
            "            // " + line.strip() + "\n"
            for line in limit.splitlines()
        )
        for name, source in (
            ("limit in line comments", spool.replace(limit, commented_limit)),
            (
                "limit in a block comment",
                spool.replace(limit, "/*\n" + limit + "*/\n"),
            ),
            (
                "limit under #if 0",
                spool.replace("#if defined(Q_OS_UNIX)", "#if 0"),
            ),
            (
                "limit in an uncalled function",
                spool.replace(limit, "")
                + "void unused_limit(const rlimit& fileLimit) {\n"
                + limit
                + "}\n",
            ),
            ("second guarded spool", spool + spool),
            (
                "second unguarded spool",
                spool + 'process.setStandardOutputFile("output.log");\n',
            ),
        ):
            with self.subTest(spool=name):
                self.assert_failure(
                    "may spool standard output only once",
                    {runner: GITHUB_CLI_PROCESS_RUNNER_SOURCE + source},
                )
        self.assert_failure(
            "Forbidden forwarded, merged, or file-redirected",
            {
                runner: GITHUB_CLI_PROCESS_RUNNER_SOURCE
                + spool
                + 'process->setStandardErrorFile("error.log");\n'
            },
        )

        for hostile_surface in (
            "process.readAll();\n",
            "process.readLine();\n",
            "process.read(buffer, size);\n",
        ):
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    "Forbidden unselected raw process channel read",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

    def test_cli_runner_rejects_ambient_environment_reads(self) -> None:
        hostile_surfaces = (
            "auto environment = "
            "QProcessEnvironment::systemEnvironment();\n",
            "QProcessEnvironment environment("
            "QProcessEnvironment::InheritFromParent);\n",
            "auto environment = process.processEnvironment();\n",
            'auto value = qgetenv("UNKNOWN_CANARY");\n',
            'auto value = qEnvironmentVariable("UNKNOWN_CANARY");\n',
            'auto* value = getenv("UNKNOWN_CANARY");\n',
            "auto** values = environ;\n",
        )
        for hostile_surface in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    "Forbidden inherited or ambient environment",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

        hostile_surfaces = (
            'const char* duplicate = "PATH";\n',
            'const char* duplicate = "SystemRoot";\n',
            'child_environment.insert("PATH", "ambient");\n',
            'child_environment.insert("SystemRoot", "ambient");\n',
        )
        for hostile_surface in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    "runner may name PATH and SystemRoot only while validating",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

        self.assert_failure(
            "neutral working-directory validation ownership violation",
            {
                "src/adapters/github/src/other.cpp":
                    'const char* repository_marker = ".git";\n'
            },
        )

    def test_cli_runner_rejects_raw_errors_and_logging(self) -> None:
        hostile_surfaces = (
            (
                "auto error = process.errorString();\n",
                "raw process error-string exposure",
            ),
            (
                "std::cerr << process.readAllStandardError();\n",
                "logging/printing",
            ),
            (
                "qWarning() << process.readAllStandardError();\n",
                "logging/printing",
            ),
            (
                'const char* forbidden = "Authorization";\n',
                "credential, status-output, cache, or debug",
            ),
            (
                'const char* forbidden = "--cache";\n',
                "credential, status-output, cache, or debug",
            ),
        )
        for hostile_surface, expected in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    f"Forbidden {expected} surface",
                    {
                        "src/adapters/github/src/"
                        "github_cli_process_runner.cpp":
                            GITHUB_CLI_PROCESS_RUNNER_SOURCE
                            + hostile_surface
                    },
                )

    def test_unix_agent_supervisor_target_is_required(self) -> None:
        self.assert_failure(
            "Agent process adapter target contract is incomplete",
            agent_target=AGENT_TARGET.replace("tool_better_pull_review_agent_supervisor", "other_helper"),
        )

    def test_windows_agent_supervisor_target_is_required(self) -> None:
        for removed, remaining in (
            (
                "    bpr_add_executable(tool_better_pull_review_agent_supervisor"
                " src/windows_agent_supervisor.cpp)\n",
                "",
            ),
            (
                "tool_better_pull_review_agent_supervisor PRIVATE Kernel32 ",
                "tool_better_pull_review_agent_supervisor PRIVATE ",
            ),
        ):
            with self.subTest(removed=removed):
                self.assertEqual(AGENT_TARGET.count(removed), 1)
                self.assert_failure(
                    "Windows agent supervisor target contract is incomplete",
                    agent_target=AGENT_TARGET.replace(removed, remaining),
                )

    def test_unix_agent_supervisor_process_authority_is_exact(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, hostile in (
            ("unix_agent_supervisor.cpp", "::execvp(program, arguments);"),
            ("unix_agent_supervisor.cpp", 'system("gh api");'),
            ("other.cpp", "::fork();"),
            ("other.cpp", "::execv(argv[1], argv + 1);"),
        ):
            with self.subTest(path=path, hostile=hostile):
                self.assert_failure(
                    "Alternate process or shell API is forbidden",
                    {f"src/adapters/agent/src/{path}": hostile},
                )

    def test_alternate_process_apis_are_rejected_everywhere(self) -> None:
        hostile_surfaces = (
            "NSTask* task;\n",
            "GSubprocess* process;\n",
            "CreateProcessW(nullptr, nullptr, nullptr, nullptr, FALSE, "
            "0U, nullptr, nullptr, nullptr, nullptr);\n",
            "ShellExecuteW(nullptr, nullptr, nullptr, nullptr, nullptr, 0);\n",
            "posix_spawn(nullptr, nullptr, nullptr, nullptr, nullptr, "
            "nullptr);\n",
            "posix_spawnp(nullptr, nullptr, nullptr, nullptr, nullptr, "
            "nullptr);\n",
            'system("gh api");\n',
            '_wsystem(L"gh api");\n',
            'popen("gh api", "r");\n',
            "fork();\n",
            "vfork();\n",
            "execvp(program, arguments);\n",
            'const char* shell = "powershell.exe";\n',
            "#include <boost/process.hpp>\n",
            "g_spawn_async(nullptr, nullptr, nullptr, 0, nullptr, nullptr, "
            "nullptr, nullptr);\n",
        )
        for hostile_surface in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                expected = (
                    "posix_spawn ownership violation"
                    if hostile_surface.startswith("posix_spawn(")
                    else (
                        "native Windows process creation API ownership violation"
                        if hostile_surface.startswith("CreateProcessW")
                        else "Alternate process or shell API is forbidden"
                    )
                )
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/alternate_process.cpp":
                            hostile_surface
                    },
                )

    def test_designated_live_public_observer_is_the_only_curl_owner(
        self,
    ) -> None:
        for hostile_surface in (
            'constexpr const char* executable = "/usr/bin/curl";\n',
            'constexpr const char* requirement = "com.apple.curl";\n',
        ):
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    "designated live public curl/network ownership violation",
                    {
                        "src/adapters/github/src/alternate_observer.cpp":
                            hostile_surface
                    },
                )

    def test_designated_live_public_observer_policy_is_hostile_cut(
        self,
    ) -> None:
        sources = {
            "src/adapters/github/src/designated_live_public_observer.cpp":
                DESIGNATED_LIVE_PUBLIC_OBSERVER_SOURCE,
            "src/adapters/github/src/designated_live_public_observer.hpp":
                DESIGNATED_LIVE_PUBLIC_OBSERVER_INTERFACE,
        }
        target = GITHUB_TARGET + DESIGNATED_LIVE_PUBLIC_OBSERVER_TARGET
        allowed = self.run_check(sources, github_target=target)
        self.assertEqual(
            allowed.returncode,
            0,
            allowed.stdout + allowed.stderr,
        )

        for required_fragment in (
            '"--disable"',
            "POSIX_SPAWN_START_SUSPENDED",
            'CFSTR("anchor apple and identifier \\"com.apple.curl\\"")',
            "std::array<char*, 1>{ nullptr }",
        ):
            with self.subTest(required_fragment=required_fragment):
                self.assert_failure(
                    "Designated live public observer policy is incomplete",
                    {
                        **sources,
                        "src/adapters/github/src/"
                        "designated_live_public_observer.cpp":
                            DESIGNATED_LIVE_PUBLIC_OBSERVER_SOURCE.replace(
                                required_fragment,
                                "",
                                1,
                            ),
                    },
                    github_target=target,
                )

        self.assert_failure(
            "Designated live public observer acquired forbidden ambient",
            {
                **sources,
                "src/adapters/github/src/designated_live_public_observer.cpp":
                    DESIGNATED_LIVE_PUBLIC_OBSERVER_SOURCE
                    + '\nconst char* forbidden = getenv("HTTPS_PROXY");\n',
            },
            github_target=target,
        )

    def test_designated_live_supervisor_observes_before_app_launch(
        self,
    ) -> None:
        sources = {
            "src/adapters/github/src/designated_live_public_observer.cpp":
                DESIGNATED_LIVE_PUBLIC_OBSERVER_SOURCE,
            "src/adapters/github/src/designated_live_public_observer.hpp":
                DESIGNATED_LIVE_PUBLIC_OBSERVER_INTERFACE,
            "src/app/CMakeLists.txt":
                DESIGNATED_LIVE_MACOS_SUPERVISOR_TARGET,
            "src/app/src/designated_live_macos_supervisor.cpp":
                DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE,
            "src/app/src/designated_live_macos_supervisor_identity.cpp":
                DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_SOURCE,
            "src/app/src/designated_live_macos_supervisor_launch.cpp":
                DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_SOURCE,
            "scripts/designated_live_macos_capsule.py":
                DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT,
            "cmake/BprDesignatedLiveStageZero.cmake":
                DESIGNATED_LIVE_STAGE_ZERO_PRELOADER,
            "cmake/BprTestIsolation.cmake":
                DESIGNATED_LIVE_TEST_ISOLATION,
        }
        target = GITHUB_TARGET + DESIGNATED_LIVE_PUBLIC_OBSERVER_TARGET
        allowed = self.run_check(sources, github_target=target)
        self.assertEqual(
            allowed.returncode,
            0,
            allowed.stdout + allowed.stderr,
        )

        self.assert_failure(
            "Designated live supervisor prelaunch policy is incomplete",
            {
                **sources,
                "src/app/src/designated_live_macos_supervisor.cpp":
                    DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE.replace(
                        "observe_designated_live_public_target()",
                        "observe_designated_live_public_target_disabled()",
                        1,
                    ),
            },
            github_target=target,
        )

        self.assert_failure(
            "Designated live supervisor prelaunch policy is incomplete",
            {
                **sources,
                "src/app/src/designated_live_macos_supervisor.cpp":
                    DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE.replace(
                        "run_static_verifier(*retainedSession)",
                        "run_static_verifier_disabled(*retainedSession)",
                        1,
                    ),
            },
            github_target=target,
        )

        for required_continuity_fragment in (
            "session::open(",
            "designated_live_supervisor_launch::verify_static(",
            "designated_live_supervisor_launch::launch(",
            "restored = channel->restore()",
            "if (!restored || !channel.has_value()",
        ):
            with self.subTest(
                required_continuity_fragment=required_continuity_fragment
            ):
                self.assert_failure(
                    "Designated live supervisor prelaunch policy is incomplete",
                    {
                        **sources,
                        "src/app/src/designated_live_macos_supervisor.cpp":
                            DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE.replace(
                                required_continuity_fragment,
                                "disabled_continuity_fragment",
                                1,
                            ),
                    },
                    github_target=target,
                )

        self.assert_failure(
            "Designated live supervisor must delegate application resume",
            {
                **sources,
                "src/app/src/designated_live_macos_supervisor.cpp":
                    DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE
                    + "\n// hostile resume: SIGCONT\n",
            },
            github_target=target,
        )

        self.assert_failure(
            "posix_spawn ownership violation",
            {
                **sources,
                "src/app/src/designated_live_macos_supervisor.cpp":
                    DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE
                    + "\nvoid hostile_spawn() { posix_spawn(nullptr, nullptr, "
                    "nullptr, nullptr, nullptr, nullptr); }\n",
            },
            github_target=target,
        )

        for required_controller_fragment in (
            "aApplication != fixedApplication",
            "aApplication.is_absolute()",
            "fixedApplication.c_str()",
            "aOperations.Barrier(phase::BeforeSpawn)",
            "aOperations.Spawn(",
            "aOperations.PostSpawnCheck(",
            "aOperations.Barrier(phase::BeforeResume)",
            "aOperations.Resume(",
            "aOperations.Refuse(",
            "aSession.configure_verifier_spawn(actions)",
            "python.c_str()",
            'std::string{ "." }',
            'std::string{ "--inventory-fd" }',
            "aSession.transmit_verifier_and_inventory(*transport)",
            "result.Revalidated = aSession.revalidate()",
        ):
            with self.subTest(
                required_controller_fragment=required_controller_fragment
            ):
                self.assert_failure(
                    "Designated live supervisor launch controller is incomplete",
                    {
                        **sources,
                        "src/app/src/designated_live_macos_supervisor_launch.cpp":
                            DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_SOURCE.replace(
                                required_controller_fragment,
                                "disabled_controller_fragment",
                                1,
                            ),
                    },
                    github_target=target,
                )

        for forbidden_verifier_path_fragment in (
            "aSession.root_path()",
            "std::filesystem::absolute(",
            "std::filesystem::canonical(",
            "std::filesystem::weakly_canonical(",
            '"--inventory"',
        ):
            with self.subTest(
                forbidden_verifier_path_fragment=forbidden_verifier_path_fragment
            ):
                self.assert_failure(
                    "Designated live supervisor verifier can regain mutable pathname",
                    {
                        **sources,
                        "src/app/src/designated_live_macos_supervisor_launch.cpp":
                            DESIGNATED_LIVE_MACOS_SUPERVISOR_LAUNCH_SOURCE
                            + "\n// hostile verifier authority: "
                            + forbidden_verifier_path_fragment
                            + "\n",
                    },
                    github_target=target,
                )

        observation = (
            "const auto observed\n"
            "\t\t= bpr::adapters::github::detail::"
            "observe_designated_live_public_target();"
        )
        # BPR-0352 added the inventory digest to this call, so the literal the
        # reorder case rewrites had to follow. It is the real source's text,
        # not an approximation of it: a stale literal here silently stops
        # rewriting anything and the case then proves nothing.
        launch = (
            "const auto launched = launch_attended_application(\n"
            "\t\tattended->Approval, attended->Profile, *retainedSession);"
        )
        reordered = DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE.replace(
            observation,
            "const auto launchResult = launch_attended_application("
            "attended->Approval, attended->Profile, *retainedSession);",
            1,
        ).replace(
            launch,
            observation
            + "\n\tif (!observed.has_value()) {\n"
            "\t\treturn EXIT_FAILURE;\n\t}\n"
            "\tconst auto launched = launchResult;",
            1,
        )
        self.assertNotEqual(
            reordered,
            DESIGNATED_LIVE_MACOS_SUPERVISOR_SOURCE,
        )
        self.assert_failure(
            "Designated live supervisor must retain identity before verification",
            {
                **sources,
                "src/app/src/designated_live_macos_supervisor.cpp": reordered,
            },
            github_target=target,
        )

        for required_identity_fragment in (
            "add_retained_directory_action(aActions, RootDescriptor)",
            "posix_spawn_file_actions_addfchdir(&aActions, aDescriptor)",
            "posix_spawn_file_actions_addfchdir_np(&aActions, aDescriptor)",
            "open_verifier(result.RootDescriptor)",
            "result.VerifierSha256 != aTrustedVerifierSha256",
            "sha256_hex(result.InventoryBytes)",
            "::socketpair(",
            "SO_NOSIGPIPE",
            "transmit_stream(",
        ):
            with self.subTest(required_identity_fragment=required_identity_fragment):
                self.assert_failure(
                    "Designated live supervisor retained identity is incomplete",
                    {
                        **sources,
                        "src/app/src/designated_live_macos_supervisor_identity.cpp":
                            DESIGNATED_LIVE_MACOS_SUPERVISOR_IDENTITY_SOURCE.replace(
                                required_identity_fragment,
                                "disabled_identity_fragment",
                            ),
                    },
                    github_target=target,
                )

        for required_retained_verifier_fragment in (
            "retained_root = arguments.inventory_fd is not None",
            'arguments.capsule != pathlib.Path(".")',
            "retained_relative_path(path.parent / target)",
            "inventory_entries(root, retained_root=retained_root)",
            "verify_dependencies(root, arguments.otool, retained_root=retained_root)",
        ):
            with self.subTest(
                required_retained_verifier_fragment=required_retained_verifier_fragment
            ):
                self.assert_failure(
                    "Designated live verifier can regain mutable pathname authority",
                    {
                        **sources,
                        "scripts/designated_live_macos_capsule.py":
                            DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
                                required_retained_verifier_fragment,
                                "disabled_retained_verifier_fragment",
                            ),
                    },
                    github_target=target,
                )

        for required_stage_zero_fragment in (
            "retained_image = retain_image(arguments.image)",
            "with signals.defer_interrupts():\n"
            "                attach_read_only("
            "retained_image.path, root, arguments.hdiutil)\n"
            "                attached = True",
            "attach_read_only(retained_image.path, root, arguments.hdiutil)",
            "retained_image = rebaseline_attached_image(retained_image)",
            '"-readonly"',
            '"-nobrowse"',
            '"-noautoopen"',
            "mounted = capture_runtime_identity(root)",
            "mount_descriptor = os.open(",
            "capsule_descriptor = os.open(",
            "filesystem_identity(os.fstat(expected.mount_descriptor))",
            "filesystem_identity(os.fstat(expected.capsule_descriptor))",
            "inventory = root / IMAGE_INVENTORY_RELATIVE",
            "helper = root / IMAGE_CAPSULE_RELATIVE / HELPER_RELATIVE",
            "launched = run_stage_one(helper, inventory, approval, signals, **profile_arguments)",
            "STAGE_ONE_TIMEOUT_SECONDS = 1500.0",
            "STAGE_ONE_TERM_GRACE_SECONDS = 10.0",
            "STAGE_ONE_KILL_GRACE_SECONDS = 10.0",
            "STAGE_ZERO_SIGNALS = "
            "(signal.SIGTERM, signal.SIGINT, signal.SIGHUP)",
            "signal.signal(signum, self.handle)",
            "signal.signal(signum, signal.SIG_IGN)",
            "subprocess.Popen(arguments, start_new_session=True)",
            "process.wait(timeout=STAGE_ONE_TIMEOUT_SECONDS)",
            "signals.defer_interrupts()\n"
            "        if signals is not None\n"
            "        else contextlib.nullcontext()",
            "signal_process_group(process_group, signal.SIGTERM)",
            "process.wait(timeout=max(0.0, term_deadline - time.monotonic()))",
            "wait_for_process_group_exit(process_group, term_deadline)",
            "signal_process_group(process_group, signal.SIGKILL)",
            "process.wait(timeout=max(0.0, kill_deadline - time.monotonic()))",
            "wait_for_process_group_exit(process_group, kill_deadline)",
            "except BaseException:\n"
            "        if process is not None and "
            "process_group_exists(process.pid):",
            "with scoped_stage_zero_signals() as signals:",
            "scope.install()",
            "scope.restore()",
            "finally:\n"
            "            signals.begin_cleanup()\n"
            "            if mounted is not None:",
            "close_runtime_identity(mounted)",
            "require_underlying_mount_root(root, underlying)",
        ):
            with self.subTest(required_stage_zero_fragment=required_stage_zero_fragment):
                hostile = DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
                    required_stage_zero_fragment,
                    "disabled_stage_zero_fragment",
                    1,
                )
                self.assertNotEqual(hostile, DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT)
                self.assert_failure(
                    "Designated live image stage zero is incomplete",
                    {
                        **sources,
                        "scripts/designated_live_macos_capsule.py": hostile,
                    },
                    github_target=target,
                )

        for required_hash_identity_fragment in (
            "filesystem_identity(os.fstat(image.descriptor)) != image.identity",
            "filesystem_identity(os.fstat(path_descriptor)) != image.identity",
        ):
            with self.subTest(
                required_hash_identity_fragment=required_hash_identity_fragment
            ):
                hostile = DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
                    required_hash_identity_fragment,
                    "disabled_hash_identity_fragment",
                    1,
                )
                self.assert_failure(
                    "hashes must be bracketed by full retained",
                    {
                        **sources,
                        "scripts/designated_live_macos_capsule.py": hostile,
                    },
                    github_target=target,
                )

        detach_line = (
            "                detach_without_force(root, arguments.hdiutil)\n"
        )
        detach_before_wait = DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
            detach_line,
            "",
            1,
        ).replace(
            "            launched = run_stage_one("
            "helper, inventory, approval, signals, **profile_arguments)\n",
            "            detach_without_force(root, arguments.hdiutil)\n"
            "            launched = run_stage_one("
            "helper, inventory, approval, signals, **profile_arguments)\n",
            1,
        )
        self.assertNotEqual(detach_before_wait, DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT)
        self.assert_failure(
            "only then detach",
            {
                **sources,
                "scripts/designated_live_macos_capsule.py": detach_before_wait,
            },
            github_target=target,
        )

        post_runtime = (
            "revalidate_runtime_identity(\n"
            "                            root,\n"
            "                            mounted,\n"
            "                            arguments.source_sha256,\n"
            "                        )"
        )
        before_last, separator, after_last = (
            DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.rpartition(post_runtime)
        )
        self.assertEqual(separator, post_runtime)
        missing_post_child_revalidation = (
            before_last + "disabled_post_child_revalidation" + after_last
        )
        self.assert_failure(
            "must revalidate before and after",
            {
                **sources,
                "scripts/designated_live_macos_capsule.py":
                    missing_post_child_revalidation,
            },
            github_target=target,
        )

        forced_detach = DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
            '[str(hdiutil), "detach", str(root), "-quiet"]',
            '[str(hdiutil), "detach", "-force", str(root), "-quiet"]',
            1,
        )
        self.assertNotEqual(forced_detach, DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT)
        self.assert_failure(
            "must never force detach",
            {
                **sources,
                "scripts/designated_live_macos_capsule.py": forced_detach,
            },
            github_target=target,
        )

        created_image_detach = (
            "        detach_created_image(arguments.output, arguments.hdiutil)\n"
        )
        image_digest = "    digest = image_sha256(arguments.output)\n"
        self.assertEqual(
            DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.count(created_image_detach), 1
        )
        self.assertEqual(DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.count(image_digest), 1)
        without_created_image_detach = DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
            created_image_detach, "        pass\n", 1
        )
        moved_after_digest = "    " + created_image_detach.lstrip()
        for regression, script in (
            ("removed", without_created_image_detach),
            (
                "moved after the digest",
                without_created_image_detach.replace(
                    image_digest, image_digest + moved_after_digest, 1
                ),
            ),
            (
                "repeated after the digest",
                DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT.replace(
                    image_digest, image_digest + moved_after_digest, 1
                ),
            ),
        ):
            with self.subTest(created_image_detach=regression):
                self.assertNotEqual(script, DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT)
                self.assert_failure(
                    "must detach its own created image exactly once",
                    {**sources, "scripts/designated_live_macos_capsule.py": script},
                    github_target=target,
                )

        for forbidden_stage_one_fragment in (
            "subprocess.run(arguments",
            "signal.pthread_sigmask",
        ):
            with self.subTest(
                forbidden_stage_one_fragment=forbidden_stage_one_fragment
            ):
                self.assert_failure(
                    "must own a bounded, signal-safe process group",
                    {
                        **sources,
                        "scripts/designated_live_macos_capsule.py":
                            DESIGNATED_LIVE_MACOS_CAPSULE_SCRIPT
                            + "\n# hostile stage-one path: "
                            + forbidden_stage_one_fragment
                            + "\n",
                    },
                    github_target=target,
                )

        for required_preloader_fragment in (
            '"/usr/bin/python3"',
            "os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW",
            "before = os.fstat(descriptor)",
            "after = os.fstat(descriptor)",
            "before.st_size > 1048576",
            "identity(before) != identity(after)",
            "hashlib.sha256(captured).hexdigest() != expected",
            'exec(compile(captured, path, "exec"), scope, scope)',
        ):
            with self.subTest(required_preloader_fragment=required_preloader_fragment):
                hostile = DESIGNATED_LIVE_STAGE_ZERO_PRELOADER.replace(
                    required_preloader_fragment,
                    "disabled_preloader_fragment",
                    1,
                )
                self.assertNotEqual(hostile, DESIGNATED_LIVE_STAGE_ZERO_PRELOADER)
                self.assert_failure(
                    "Designated live stage-zero protected loader is incomplete",
                    {
                        **sources,
                        "cmake/BprDesignatedLiveStageZero.cmake": hostile,
                    },
                    github_target=target,
                )

        compact_test_isolation = " ".join(
            DESIGNATED_LIVE_TEST_ISOLATION.split()
        )
        required_manifest_capture_fragments = (
            'file( READ "${bpr_designated_live_source_manifest}" '
            "bpr_designated_live_source_manifest_bytes LIMIT 1048577 )",
            "string( SHA256 bpr_designated_live_source_manifest_sha256 "
            '"${bpr_designated_live_source_manifest_bytes}" )',
            'string( SUBSTRING "${bpr_designated_live_source_manifest_bytes}" '
            '"${bpr_designated_live_source_manifest_last}" 1 '
            "bpr_designated_live_source_manifest_terminator )",
            'string( SUBSTRING "${bpr_designated_live_source_manifest_bytes}" '
            '0 "${bpr_designated_live_source_manifest_last}" '
            "bpr_designated_live_source_manifest_body )",
            'string( REPLACE "\\n" ";" bpr_designated_live_source_entries '
            '"${bpr_designated_live_source_manifest_body}" )',
        )
        for required_manifest_capture_fragment in (
            required_manifest_capture_fragments
        ):
            with self.subTest(
                required_manifest_capture_fragment=
                    required_manifest_capture_fragment
            ):
                self.assertIn(
                    required_manifest_capture_fragment,
                    compact_test_isolation,
                )
                hostile = compact_test_isolation.replace(
                    required_manifest_capture_fragment,
                    "disabled_manifest_capture_fragment",
                    1,
                )
                self.assert_failure(
                    "must hash and parse one bounded capture",
                    {
                        **sources,
                        "cmake/BprTestIsolation.cmake": hostile,
                    },
                    github_target=target,
                )

        for forbidden_manifest_reopen in (
            'file( SHA256 "${bpr_designated_live_source_manifest}"',
            'file( STRINGS "${bpr_designated_live_source_manifest}"',
            'file( TIMESTAMP "${bpr_designated_live_source_manifest}"',
        ):
            with self.subTest(forbidden_manifest_reopen=forbidden_manifest_reopen):
                self.assert_failure(
                    "must not be reopened after capture",
                    {
                        **sources,
                        "cmake/BprTestIsolation.cmake":
                            DESIGNATED_LIVE_TEST_ISOLATION
                            + "\n"
                            + forbidden_manifest_reopen
                            + " hostile)\n",
                    },
                    github_target=target,
                )

        for direct_plain_command in (
            "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}",
            "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}",
        ):
            with self.subTest(direct_plain_command=direct_plain_command):
                direct_plain_registration = (
                    DESIGNATED_LIVE_TEST_ISOLATION
                    + "\nadd_test(NAME hostile COMMAND "
                    + direct_plain_command
                    + ")\n"
                )
                self.assert_failure(
                    "must not register a plain supervisor or inventory",
                    {
                        **sources,
                        "cmake/BprTestIsolation.cmake": direct_plain_registration,
                    },
                    github_target=target,
                )

    def test_cli_transport_requires_allowlisted_invocation_and_environment(
        self,
    ) -> None:
        required_fragments = (
            '"api"',
            '"--hostname"',
            '"--include"',
            '"--method"',
            '"--input"',
            '".git"',
            '"GH_PROMPT_DISABLED=1"',
            '"GH_NO_UPDATE_NOTIFIER=1"',
            '"GH_NO_EXTENSION_UPDATE_NOTIFIER=1"',
            '"GH_SPINNER_DISABLED=1"',
            '"NO_COLOR=1"',
            '"CLICOLOR=0"',
            '"HOME"',
            '"XDG_RUNTIME_DIR"',
            '"DBUS_SESSION_BUS_ADDRESS"',
            '"USERPROFILE"',
            '"APPDATA"',
            '"LOCALAPPDATA"',
            '"SystemRoot"',
            '"PATH"',
            '"HTTPS_PROXY"',
            '"NO_PROXY"',
            '"SSL_CERT_FILE"',
            '"SSL_CERT_DIR"',
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "GitHub CLI transport must retain explicit allowlisted",
                    {
                        "src/adapters/github/src/gh_cli_transport.cpp":
                            GH_CLI_TRANSPORT_SOURCE.replace(
                                fragment,
                                '"REMOVED_REQUIRED_TRANSPORT_PROPERTY"',
                            )
                    },
                )

    def test_cli_invocation_and_environment_construction_has_one_owner(
        self,
    ) -> None:
        hostile_literals = (
            '"api"',
            '"--hostname"',
            '"--method"',
            '"--input"',
            '"GH_PROMPT_DISABLED=1"',
            '"GH_NO_UPDATE_NOTIFIER=1"',
            '"GH_NO_EXTENSION_UPDATE_NOTIFIER=1"',
            '"GH_SPINNER_DISABLED=1"',
            '"NO_COLOR=1"',
            '"CLICOLOR=0"',
            '"HOME"',
            '"XDG_RUNTIME_DIR"',
            '"DBUS_SESSION_BUS_ADDRESS"',
            '"USERPROFILE"',
            '"APPDATA"',
            '"LOCALAPPDATA"',
            '"SystemRoot"',
            '"PATH"',
            '"HTTPS_PROXY"',
            '"NO_PROXY"',
            '"SSL_CERT_FILE"',
            '"SSL_CERT_DIR"',
        )
        for hostile_literal in hostile_literals:
            with self.subTest(hostile_literal=hostile_literal):
                self.assert_failure(
                    "environment construction ownership violation",
                    {
                        "src/adapters/github/src/other.cpp":
                            f"const char* hostile = {hostile_literal};\n"
                    },
                )

    def test_platform_directory_variables_may_be_read_for_discovery(
        self,
    ) -> None:
        # BPR-0894. The transport owns *constructing* the child process
        # environment. Reading one of these variables to find the directory the
        # platform documents for the purpose is discovery, not construction, and
        # the rule used to forbid it because it matched the bare token. BPR-0881
        # hit this and probed /run/user/<uid> directly instead.
        for variable in (
            "XDG_RUNTIME_DIR",
            "USERPROFILE",
            "APPDATA",
            "LOCALAPPDATA",
        ):
            with self.subTest(variable=variable):
                result = self.run_check(
                    {
                        "src/adapters/sqlite/src/discovery.cpp":
                            "const auto runtime = "
                            f'environment_directory("{variable}");\n'
                    }
                )
                self.assertEqual(
                    result.returncode, 0, result.stdout + result.stderr
                )

    def test_platform_directory_variables_stay_owned_for_construction(
        self,
    ) -> None:
        # The other half of BPR-0894: narrowing the rule must not stop it
        # failing a real leak of the transport's environment construction. A
        # bare literal, an assignment and a name appended to a child
        # environment are all still the transport's alone -- including in a file
        # that legitimately reads the same variable for discovery elsewhere.
        for variable in (
            "XDG_RUNTIME_DIR",
            "USERPROFILE",
            "APPDATA",
            "LOCALAPPDATA",
        ):
            for hostile_form in (
                f'const char* hostile = "{variable}";\n',
                f'setenv("{variable}", value, 1);\n',
                f'append_environment(environment, "{variable}", value);\n',
                f'const auto found = environment_directory("{variable}");\n'
                f'append_environment(environment, "{variable}", *found);\n',
            ):
                with self.subTest(variable=variable, form=hostile_form):
                    self.assert_failure(
                        "environment construction ownership violation",
                        {
                            "src/adapters/github/src/other.cpp": hostile_form
                        },
                    )

    def test_platform_directory_discovery_cannot_launder_construction(
        self,
    ) -> None:
        # BPR-0894, second landing. Independent review defeated the first one
        # twice, and both holes are here so they cannot come back.
        #
        # The first: the accessor spelling was unanchored, so any name *ending*
        # in environment_directory was stripped with it. The transport's own
        # convention already contains canonical_environment_directory, so this
        # was a plausible accident rather than a contrived one.
        #
        # The second: the accessor is a file-local function, so a file can
        # define its own that constructs instead of reading. The spelling alone
        # is not a guarantee, which is why a file may discover or may build
        # child environments but only the transport may do both.
        laundering_forms = (
            'child.inherit_environment_directory("APPDATA");\n',
            'pass_environment_directory("LOCALAPPDATA");\n',
            "request.Environment.push_environment_directory"
            '("USERPROFILE");\n',
            "#define environment_directory(name) "
            "append_environment(environment, name, std::getenv(name))\n"
            'environment_directory("XDG_RUNTIME_DIR");\n',
            "static auto environment_directory(const char* name) { "
            "request.Environment.push_back({ name, std::getenv(name) }); }\n"
            'void build() { environment_directory("USERPROFILE"); }\n',
        )
        for laundering_form in laundering_forms:
            with self.subTest(form=laundering_form):
                self.assert_failure(
                    "environment construction ownership violation",
                    {
                        "src/adapters/github/src/other.cpp": laundering_form
                    },
                )

    def test_cli_transport_rejects_credential_status_cache_and_debug(
        self,
    ) -> None:
        hostile_literals = (
            '"--auth-token"',
            '"--with-token"',
            '"--token"',
            '"--cache"',
            '"--verbose"',
            '"--debug"',
            '"--jq"',
            '"--template"',
            '"--paginate"',
            '"--slurp"',
            '"--preview"',
            '"--field"',
            '"--raw-field"',
            '"--silent"',
            '"Authorization"',
            '"GH_TOKEN"',
            '"GITHUB_TOKEN"',
            '"GH_ENTERPRISE_TOKEN"',
            '"GITHUB_ENTERPRISE_TOKEN"',
            '"GH_DEBUG"',
        )
        for hostile_literal in hostile_literals:
            with self.subTest(hostile_literal=hostile_literal):
                expected = (
                    "Forbidden credential or debug literal surface"
                    if not hostile_literal.startswith('"--')
                    else "Forbidden credential, cache, debug"
                )
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/gh_cli_transport.cpp":
                            GH_CLI_TRANSPORT_SOURCE
                            + f"const char* hostile = {hostile_literal};\n"
                    },
                )

    def test_cli_transport_rejects_qt_and_logging(self) -> None:
        hostile_surfaces = (
            (
                "#include <QString>\nQString raw;\n",
                "Qt/process implementation",
            ),
            (
                "std::cerr << standard_error;\n",
                "logging/printing",
            ),
            (
                "qWarning() << standard_error;\n",
                "logging/printing",
            ),
        )
        for hostile_surface, expected in hostile_surfaces:
            with self.subTest(hostile_surface=hostile_surface):
                self.assert_failure(
                    f"Forbidden {expected} surface",
                    {
                        "src/adapters/github/src/gh_cli_transport.cpp":
                            GH_CLI_TRANSPORT_SOURCE
                            + hostile_surface
                    },
                )

    def test_linux_secret_service_source_is_required(self) -> None:
        self.assert_failure(
            "designated Linux Secret Service source is required",
            omit_linux_source=True,
        )

    def test_linux_source_requires_exact_linux_target_ownership(self) -> None:
        hostile_targets = (
            (
                GITHUB_TARGET.replace(
                    "            src/linux_secret_service.cpp\n",
                    "",
                ),
                "Linux GitHub adapter must compile",
            ),
            (
                GITHUB_TARGET.replace(
                    'if(CMAKE_SYSTEM_NAME STREQUAL "Linux")\n',
                    "if(UNIX AND NOT APPLE)\n",
                ),
                "must define an exact Linux branch",
            ),
            (
                GITHUB_TARGET
                + "target_sources(\n"
                "    better_pull_review_adapter_github\n"
                "    INTERFACE src/linux_secret_service.cpp\n"
                ")\n",
                "must remain Linux-only",
            ),
        )
        for target, expected_message in hostile_targets:
            with self.subTest(expected_message=expected_message):
                self.assert_failure(
                    expected_message,
                    github_target=target,
                )

    def test_linux_target_requires_imported_libsecret(self) -> None:
        hostile_targets = (
            (
                GITHUB_TARGET.replace(
                    "            LibSecret::LibSecret\n",
                    "",
                ),
                "must link the imported libsecret target",
            ),
        )
        for target, expected_message in hostile_targets:
            with self.subTest(expected_message=expected_message):
                self.assert_failure(
                    expected_message,
                    github_target=target,
                )

    def test_linux_build_ownership_cannot_move_to_another_target(self) -> None:
        self.assert_failure(
            "Linux Secret Service build ownership must remain",
            git_target=GIT_TARGET
            + "\ntarget_sources(better_pull_review_adapter_git PRIVATE "
            "src/linux_secret_service.cpp)\n",
        )

    def test_linux_vault_requires_the_effect_executor(self) -> None:
        self.assert_failure(
            "Linux Secret Service adapter requires the host effect executor",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    LINUX_SECRET_SERVICE_SOURCE.replace(
                        "application::effect_executor* "
                        "selected_effect_lane = nullptr;\n",
                        "",
                    )
            },
        )

    def test_linux_vault_rejects_the_decision_executor(self) -> None:
        self.assert_failure(
            "Forbidden serialized decision-executor surface",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    LINUX_SECRET_SERVICE_SOURCE
                    + "application::decision_executor* "
                    "forbidden_decision_lane = nullptr;\n"
            },
        )

    def test_linux_vault_requires_exact_service_and_session(self) -> None:
        required_fragments = (
            '"org.freedesktop.secrets"',
            "secret_service_open_sync",
            "non_prompting_secret_service_get_type()",
            "c_str(canonical_service_bus_name),\n"
            "        SECRET_SERVICE_NONE",
            "SECRET_SERVICE_NONE",
            "const auto failure = mapped_error(error.get())",
            "                        operation_failed\n"
            "                ? application::github_credential_vault_failure::"
            "unavailable\n"
            "                : failure",
            "g_dbus_proxy_get_name",
            "actual_name == nullptr",
            "               c_str(canonical_service_bus_name))\n"
            "            != 0",
            "g_object_unref(Service)",
            "Service = nullptr",
            "application::github_credential_vault_failure::unavailable",
            "secret_service_ensure_session_sync",
            "session_error.out()",
            "const auto failure = mapped_error(session_error.get())",
            "secret_collection_for_alias_sync",
            "SECRET_COLLECTION_DEFAULT",
            "SECRET_COLLECTION_NONE",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "must retain the selected exact",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE.replace(
                                fragment,
                                "REMOVED_REQUIRED_SERVICE_SELECTION",
                            )
                    },
                )

        hostile_source = LINUX_SECRET_SERVICE_SOURCE.replace(
            "            != 0) {",
            "            == 0) {",
        )
        self.assert_failure(
            "must retain the selected exact",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    hostile_source
            },
        )

        open_session_source = LINUX_SECRET_SERVICE_SOURCE.replace(
            "        SECRET_SERVICE_NONE,\n",
            "        SECRET_SERVICE_OPEN_SESSION,\n",
            1,
        )
        self.assert_failure(
            "must retain the selected exact",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    open_session_source
            },
        )

        session_block = (
            "    error_owner session_error;\n"
            "    if(!secret_service_ensure_session_sync(\n"
            "           Service,\n"
            "           nullptr,\n"
            "           session_error.out())) {\n"
            "        const auto failure = mapped_error(session_error.get());\n"
            "        g_object_unref(Service);\n"
            "        Service = nullptr;\n"
            "        return std::unexpected{\n"
            "            failure\n"
            "                    == application::"
            "github_credential_vault_failure::\n"
            "                        operation_failed\n"
            "                ? application::github_credential_vault_failure::"
            "unavailable\n"
            "                : failure};\n"
            "    }\n"
        )
        ensure_before_name_source = LINUX_SECRET_SERVICE_SOURCE.replace(
            session_block,
            "",
            1,
        ).replace(
            "    const auto* actual_name =\n",
            session_block + "    const auto* actual_name =\n",
            1,
        )
        self.assert_failure(
            "Linux Secret Service session ordering invalid",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    ensure_before_name_source
            },
        )

        ensure_without_normalization = session_block.replace(
            "                ? application::github_credential_vault_failure::"
            "unavailable\n"
            "                : failure",
            "                ? failure\n"
            "                : failure",
            1,
        )
        self.assert_failure(
            "Linux Secret Service failure normalization invalid",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    LINUX_SECRET_SERVICE_SOURCE.replace(
                        session_block,
                        ensure_without_normalization,
                        1,
                )
            },
        )

        ensure_without_cleanup = session_block.replace(
            "        g_object_unref(Service);\n",
            "",
            1,
        )
        self.assert_failure(
            "Linux Secret Service ensure-session failure cleanup invalid",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    LINUX_SECRET_SERVICE_SOURCE.replace(
                        session_block,
                        ensure_without_cleanup,
                        1,
                    )
            },
        )

        missing_operation_gate = LINUX_SECRET_SERVICE_SOURCE.replace(
            "    const auto service = service_proxy();\n",
            "",
            1,
        )
        self.assert_failure(
            "must verify the selected service",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    missing_operation_gate
            },
        )

    def test_linux_vault_requires_exact_schema_and_authority(self) -> None:
        required_fragments = (
            "static_assert("
            "linux_secret_service_authority_account_bytes == 574U)",
            "org.betterpullreview.github-native-credential."
            "secret-service.v2",
            "secret-service.integration-test.v2",
            'authority_attribute = "authority"',
            'schema_attribute = "xdg:schema"',
            "SECRET_SCHEMA_NONE",
            "SECRET_SCHEMA_ATTRIBUTE_STRING",
            "g_hash_table_size(attributes) != 2U",
            "nibble < 16U",
            "authority.Profile.value()",
            "authority.ProfileRevision.value()",
            "authority.Host.value()",
            "authority.Account.value()",
            "append_hex_u8(\n"
            "        output,\n"
            "        offset,\n"
            "        server_contract_discriminator(authority.ServerContract))",
            "authority.Hostname.value()",
            "static_cast<std::uint8_t>(hostname.size())",
            "index < application::maximum_github_hostname_bytes",
            "static_cast<unsigned char>(hostname[index])",
            "append_hex_u8(encoded, offset, value)",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "must retain the selected exact",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE.replace(
                                fragment,
                                "REMOVED_REQUIRED_AUTHORITY",
                            )
                    },
                )

    def test_linux_vault_requires_binary_secret_value_lifecycle(self) -> None:
        required_fragments = (
            "#include <libsecret/secret.h>",
            '"application/octet-stream"',
            'gnome_keyring_content_type = "text/plain"',
            "secret_value_new(\n"
            "        reinterpret_cast<const char*>(bytes.data()),\n"
            "        static_cast<gssize>(bytes.size()),\n"
            "        c_str(binary_content_type))",
            "secret_value_new",
            "secret_value_get_content_type",
            "recognized_content_type",
            "std::strcmp(content_type, c_str(binary_content_type)) == 0",
            "                   c_str(gnome_keyring_content_type))\n"
            "                == 0",
            "!recognized_content_type",
            "secret_value_get",
            "secret_value_unref",
            "application::maximum_github_credential_bytes",
            "secret_collection_search_sync",
            "SECRET_SEARCH_ALL",
            "secret_item_create_sync",
            "SECRET_ITEM_CREATE_REPLACE",
            "secret_item_load_secret_sync",
            "secret_item_get_secret",
            "secret_item_delete_sync",
            "secret_collection_get_locked",
            "secret_item_get_locked",
            "consteval auto c_str(const std::string_view text)",
            "text.data()[text.size()] !=",
            'throw "a string_view handed to a libsecret entry point '
            'must be "',
            "const_cast<char*>(c_str(authority_attribute))",
            "g_hash_table_lookup(attributes, c_str(authority_attribute))",
            "g_hash_table_lookup(attributes, c_str(schema_attribute))",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "must retain the selected exact",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE.replace(
                                fragment,
                                "REMOVED_REQUIRED_LIFECYCLE",
                            )
                    },
                )

        broader_recognition = LINUX_SECRET_SERVICE_SOURCE.replace(
            "                == 0);",
            "                == 0\n"
            "            || std::strcmp(content_type, "
            '"application/json") == 0);',
        )
        self.assert_failure(
            "must retain the selected exact",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    broader_recognition
            },
        )

        gnome_keyring_storage = LINUX_SECRET_SERVICE_SOURCE.replace(
            "        c_str(binary_content_type));",
            "        c_str(gnome_keyring_content_type));",
            1,
        )
        self.assert_failure(
            "must store only application/octet-stream",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    gnome_keyring_storage
            },
        )

        broad_bypasses = (
            "g_ascii_strcasecmp(content_type, expected);\n",
            'g_str_has_prefix(content_type, "text/");\n',
            'strstr(content_type, "octet-stream");\n',
            'const char* wildcard = "text/*";\n',
        )
        for hostile_source in broad_bypasses:
            with self.subTest(hostile_source=hostile_source):
                self.assert_failure(
                    "Forbidden broad SecretValue content-type bypass",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_requires_no_prompt_overrides_and_dismiss(self) -> None:
        required_fragments = (
            "SECRET_TYPE_SERVICE",
            "parent->prompt_sync = deny_prompt_sync",
            "parent->prompt_async = deny_prompt_async",
            "parent->prompt_finish = deny_prompt_finish",
            "g_dbus_proxy_call_sync",
            '"Dismiss"',
            'g_variant_new("()")',
            "G_DBUS_CALL_FLAGS_NO_AUTO_START",
            "prompt_dismiss_timeout_milliseconds",
            "SECRET_ERROR_IS_LOCKED",
        )
        for fragment in required_fragments:
            with self.subTest(fragment=fragment):
                self.assert_failure(
                    "must retain the selected exact",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE.replace(
                                fragment,
                                "REMOVED_REQUIRED_PROMPT_POLICY",
                            )
                    },
                )

        self.assert_failure(
            "must retain the selected exact",
            {
                "src/adapters/github/src/linux_secret_service.cpp":
                    LINUX_SECRET_SERVICE_SOURCE.replace(
                        "prompt_dismiss_timeout_milliseconds = 1'000",
                        "prompt_dismiss_timeout_milliseconds = 0",
                    )
            },
        )

    def test_linux_vault_requires_dismiss_on_sync_and_async_prompt_paths(
        self,
    ) -> None:
        hostile_sources = (
            LINUX_SECRET_SERVICE_SOURCE.replace(
                "void deny_prompt_sync(SecretPrompt* prompt) {\n"
                "    dismiss_prompt(prompt);\n"
                "}\n",
                "void deny_prompt_sync(SecretPrompt*) {}\n",
            ),
            LINUX_SECRET_SERVICE_SOURCE.replace(
                "void deny_prompt_async(SecretPrompt* prompt) {\n"
                "    dismiss_prompt(prompt);\n"
                "}\n",
                "void deny_prompt_async(SecretPrompt*) {}\n",
            ),
        )
        for source in hostile_sources:
            with self.subTest(source=source):
                self.assert_failure(
                    "must retain the selected exact",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            source
                    },
                )

    def test_linux_vault_rejects_environment_and_service_redirection(
        self,
    ) -> None:
        hostile_surfaces = (
            'getenv("SECRET_SERVICE_BUS_NAME");\n',
            'const char* variable = "SECRET_SERVICE_BUS_NAME";\n',
            'const char* bus = "DBUS_SESSION_BUS_ADDRESS";\n',
            "g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);\n",
            "GDBusConnection* alternate_connection = nullptr;\n",
            "GObjectConstructParam* redirected_properties = nullptr;\n",
            "g_value_set_string(value, redirected_name);\n",
            'g_object_set(object, "g-name", "org.example.secrets", '
            "nullptr);\n",
            'g_object_set_property(object, "g-name", redirected_value);\n',
            "secret_service_open_sync(other_type, redirected_name, "
            "SECRET_SERVICE_OPEN_SESSION, nullptr, nullptr);\n",
        )
        for hostile_source in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                expected_message = (
                    "must open exactly one"
                    if "secret_service_open_sync" in hostile_source
                    else (
                        "Forbidden ineffective service-name property override"
                        if (
                            '"g-name"' in hostile_source
                            or "GObjectConstructParam" in hostile_source
                            or "g_value_set_string" in hostile_source
                        )
                        else "Forbidden"
                    )
                )
                self.assert_failure(
                    expected_message,
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_rejects_convenience_unlock_and_collection_apis(
        self,
    ) -> None:
        hostile_surfaces = (
            "secret_password_store_sync(schema, nullptr);\n",
            "secret_service_get_sync(SECRET_SERVICE_OPEN_SESSION, "
            "nullptr, nullptr);\n",
            "secret_service_store_sync(service, schema, attributes, "
            "nullptr, value, nullptr, nullptr);\n",
            "secret_service_clear_sync(service, schema, attributes, "
            "nullptr, nullptr);\n",
            "const auto flags = SECRET_SEARCH_UNLOCK;\n",
            "const auto flags = SECRET_SEARCH_LOAD_SECRETS;\n",
            "secret_service_unlock_sync(service, objects, nullptr, "
            "nullptr, nullptr);\n",
            "secret_collection_create_sync(service, label, alias, "
            "SECRET_COLLECTION_CREATE_NONE, nullptr, nullptr);\n",
            'g_dbus_proxy_call_sync(proxy, "CreateCollection", nullptr, '
            "nullptr, 0, 1, nullptr, nullptr);\n",
            "secret_item_set_secret(item, value);\n",
        )
        for hostile_source in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                expected_message = (
                    "libsecret effect outside"
                    if "secret_item_set_secret" in hostile_source
                    else "Forbidden"
                )
                self.assert_failure(
                    expected_message,
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_rejects_interactive_prompt_paths(self) -> None:
        hostile_surfaces = (
            'g_dbus_proxy_call_sync(prompt, "Prompt", nullptr, nullptr, '
            "0, 1, nullptr, nullptr);\n",
            "secret_service_prompt_sync(service, prompt, nullptr, "
            "nullptr, nullptr);\n",
            'const char* method = "org.freedesktop.Secret.Prompt.Prompt";\n',
        )
        for hostile_source in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                self.assert_failure(
                    "Forbidden interactive prompt surface",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_rejects_other_native_vaults(self) -> None:
        hostile_surfaces = (
            ("QtKeychain::Job* job = nullptr;\n", "alternate credential vault"),
            ("KWallet::Wallet* wallet = nullptr;\n", "alternate credential vault"),
            ("gnome_keyring_store_password_sync();\n",
             "alternate credential vault"),
            ("SecItemAdd(query, nullptr);\n", "macOS Keychain API ownership"),
            ("CredWriteW(&credential, 0U);\n",
             "Windows Credential Manager API ownership"),
        )
        for hostile_source, expected_message in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                self.assert_failure(
                    expected_message,
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_rejects_fallback_effect_families(self) -> None:
        hostile_surfaces = (
            ("std::ofstream plaintext_file;\n", "filesystem"),
            ("write(descriptor, bytes, size);\n", "filesystem"),
            ("g_file_set_contents(path, bytes, size, nullptr);\n", "filesystem"),
            ("sqlite3* database = nullptr;\n", "settings/database"),
            ("QSettings* settings = nullptr;\n", "settings/database"),
            ("GKeyFile* settings_file = nullptr;\n", "settings/database"),
            ("g_spawn_sync(nullptr, nullptr, nullptr, 0, nullptr, nullptr, "
             "nullptr, nullptr, nullptr, nullptr);\n", "command/process"),
            ("GSubprocess* process = nullptr;\n", "command/process"),
            (
                "QNetworkAccessManager* network = nullptr;\n",
                "QNetworkAccessManager ownership violation",
            ),
            ("GSocket* socket = nullptr;\n", "network"),
            ("GtkWidget* dialog = nullptr;\n", "Qt/UI"),
            ("std::cerr << status;\n", "logging/printing"),
            ('g_debug("secret");\n', "logging/printing"),
            ("auto* fallback = alternate_vault();\n", "credential fallback"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                expected_message = (
                    surface_name
                    if "ownership violation" in surface_name
                    else f"Forbidden {surface_name} surface"
                )
                self.assert_failure(
                    expected_message,
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_linux_vault_rejects_raw_error_text(self) -> None:
        hostile_surfaces = (
            "g_dbus_error_get_remote_error(error);\n",
            "g_quark_to_string(error->domain);\n",
            "std::to_string(error->code);\n",
        )
        for hostile_source in hostile_surfaces:
            with self.subTest(hostile_source=hostile_source):
                self.assert_failure(
                    "Forbidden raw operating-system error formatting surface",
                    {
                        "src/adapters/github/src/linux_secret_service.cpp":
                            LINUX_SECRET_SERVICE_SOURCE + hostile_source
                    },
                )

    def test_windows_credential_manager_source_is_required(self) -> None:
        self.assert_failure(
            "designated Windows Credential Manager source is required",
            omit_windows_source=True,
        )

    def test_windows_source_must_be_owned_by_win32_target(self) -> None:
        self.assert_failure(
            "Windows GitHub adapter must compile",
            github_target=GITHUB_TARGET.replace(
                "            src/windows_credential_manager.cpp\n",
                "",
            ),
        )

    def test_win32_target_requires_advapi32(self) -> None:
        self.assert_failure(
            "must link Advapi32",
            github_target=GITHUB_TARGET.replace(
                "            Advapi32\n",
                "",
            ),
        )

    def test_windows_source_requires_a_win32_target_branch(self) -> None:
        self.assert_failure(
            "must define a WIN32 branch",
            github_target=GITHUB_TARGET.replace(
                WINDOWS_CREDENTIAL_TARGET,
                "",
            ),
        )

    def test_selected_windows_credential_manager_source_is_allowed(
        self,
    ) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_windows_vault_requires_the_effect_executor(self) -> None:
        self.assert_failure(
            "requires the host effect executor",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                        "effect_executor",
                        "removed_executor",
                    )
            },
        )

    def test_windows_vault_rejects_the_decision_executor(self) -> None:
        self.assert_failure(
            "Forbidden serialized decision-executor surface",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "application::decision_executor* "
                    "forbidden_decision_lane = nullptr;\n"
            },
        )

    def test_windows_vault_requires_selected_wide_lifecycle(self) -> None:
        required_lines = (
            "#include <wincred.h>\n",
            "    CredWriteW(&credential, 0U);\n",
            "    CredReadW(target, CRED_TYPE_GENERIC, 0U, &retrieved);\n",
            "    CredDeleteW(target, CRED_TYPE_GENERIC, 0U);\n",
            "    CredFree(retrieved);\n",
            "    const DWORD status = GetLastError();\n",
            "    SecureZeroMemory(credential.CredentialBlob, blob_size);\n",
        )
        for required_line in required_lines:
            with self.subTest(required_line=required_line):
                self.assert_failure(
                    "selected wide-character",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                required_line,
                                "",
                            )
                    },
                )

    def test_windows_vault_enforces_selected_supported_upper_bounds(
        self,
    ) -> None:
        bound_mutations = (
            (
                "    static_assert(667U <= "
                "CRED_MAX_GENERIC_TARGET_NAME_LENGTH);\n",
                "    static_assert(667U == "
                "CRED_MAX_GENERIC_TARGET_NAME_LENGTH);\n",
            ),
            (
                "constexpr std::size_t "
                "windows_credential_manager_maximum_blob_bytes =\n"
                "    5U * 512U;\n",
                "",
            ),
            (
                "    application::maximum_github_credential_bytes\n"
                "    == windows_credential_manager_maximum_blob_bytes);\n",
                "",
            ),
            (
                "        credential.CredentialBlobSize\n"
                "        <= windows_credential_manager_maximum_blob_bytes)\n",
                "",
            ),
        )
        for bound_line, replacement in bound_mutations:
            with self.subTest(bound_line=bound_line):
                self.assert_failure(
                    "Windows credential bound missing",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                bound_line,
                                replacement,
                            )
                    },
                )

    def test_windows_vault_requires_generic_local_bounded_properties(
        self,
    ) -> None:
        required_constants = (
            "CRED_TYPE_GENERIC",
            "CRED_PERSIST_LOCAL_MACHINE",
            "CRED_MAX_GENERIC_TARGET_NAME_LENGTH",
            "windows_credential_manager_maximum_blob_bytes",
        )
        for required_constant in required_constants:
            with self.subTest(required_constant=required_constant):
                self.assert_failure(
                    "bounded credential property",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE.replace(
                                required_constant,
                                "REMOVED_REQUIRED_PROPERTY",
                            )
                    },
                )

    def test_windows_vault_rejects_toolchain_varying_legacy_blob_bound(
        self,
    ) -> None:
        self.assert_failure(
            "Forbidden toolchain-varying legacy credential-blob bound",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "static_assert(512U <= "
                    "CRED_MAX_CREDENTIAL_BLOB_SIZE);\n"
            },
        )

    def test_windows_vault_rejects_ansi_and_neutral_aliases(self) -> None:
        forbidden_aliases = (
            "CredWrite(&credential, 0U);\n",
            "CredWriteA(&credential, 0U);\n",
            "CredRead(target, 1U, 0U, &retrieved);\n",
            "CredReadA(target, 1U, 0U, &retrieved);\n",
            "CredDelete(target, 1U, 0U);\n",
            "CredDeleteA(target, 1U, 0U);\n",
            "CREDENTIAL forbidden_neutral{};\n",
            "CREDENTIALA forbidden_ansi{};\n",
            "PCREDENTIAL forbidden_neutral_pointer = nullptr;\n",
            "PCREDENTIALA forbidden_ansi_pointer = nullptr;\n",
        )
        for forbidden_alias in forbidden_aliases:
            with self.subTest(forbidden_alias=forbidden_alias):
                self.assert_failure(
                    "Forbidden ANSI/encoding-neutral Credential Manager "
                    "surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + forbidden_alias
                    },
                )

    def test_windows_vault_rejects_unselected_credential_apis(self) -> None:
        forbidden_apis = (
            "CredUIPromptForWindowsCredentialsW();\n",
            "CredEnumerateW(nullptr, 0U, nullptr, nullptr);\n",
            "CredFindBestCredentialW(nullptr, 1U, 0U, nullptr);\n",
            "CredGetTargetInfoW(nullptr, 0U, nullptr);\n",
            "CredReadDomainCredentialsW(nullptr, 0U, nullptr, nullptr);\n",
            "CredWriteDomainCredentialsW(nullptr, nullptr, 0U);\n",
            "CredIsProtectedW(nullptr, nullptr);\n",
            "CredProtectW(FALSE, nullptr, 0U, nullptr, nullptr, nullptr);\n",
            "CredUnprotectW(FALSE, nullptr, 0U, nullptr, nullptr);\n",
            "CredRenameW(nullptr, nullptr, 1U, 0U);\n",
            "CredMarshalCredentialW(1U, nullptr, nullptr);\n",
            "CredUnmarshalCredentialW(nullptr, nullptr, nullptr);\n",
        )
        for forbidden_api in forbidden_apis:
            with self.subTest(forbidden_api=forbidden_api):
                self.assert_failure(
                    "Forbidden unselected Credential Manager surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + forbidden_api
                    },
                )

    def test_windows_vault_rejects_unselected_credential_effects(self) -> None:
        self.assert_failure(
            "credential effect outside",
            {
                "src/adapters/github/src/windows_credential_manager.cpp":
                    WINDOWS_CREDENTIAL_MANAGER_SOURCE
                    + "CredGetSessionTypes(nullptr, 0U);\n"
            },
        )

    def test_windows_vault_rejects_dpapi_and_credential_locker(self) -> None:
        hostile_surfaces = (
            ("CryptProtectData(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "0U, nullptr);\n", "DPAPI"),
            ("CryptUnprotectData(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "0U, nullptr);\n", "DPAPI"),
            ("NCryptProtectSecret(nullptr, 0U, nullptr, 0U, nullptr, nullptr, "
             "nullptr, nullptr);\n", "DPAPI"),
            ("PasswordVault forbidden_vault;\n", "Credential Locker"),
            ("Windows::Security::Credentials::PasswordCredential "
             "forbidden_credential;\n", "Credential Locker"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + hostile_source
                    },
                )

    def test_windows_vault_rejects_alternate_storage_policy(self) -> None:
        hostile_surfaces = (
            ("CRED_PERSIST_SESSION", "non-local Credential Manager "
             "persistence"),
            ("CRED_PERSIST_ENTERPRISE", "non-local Credential Manager "
             "persistence"),
            ("CRED_PRESERVE_CREDENTIAL_BLOB", "credential-blob "
             "preservation"),
            ("CRED_TYPE_DOMAIN_PASSWORD", "non-generic credential type"),
        )
        for hostile_constant, surface_name in hostile_surfaces:
            with self.subTest(hostile_constant=hostile_constant):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + f"const DWORD forbidden = {hostile_constant};\n"
                    },
                )

    def test_windows_vault_rejects_fallback_effect_families(self) -> None:
        hostile_surfaces = (
            ("RegSetValueExW(nullptr, nullptr, 0U, 0U, nullptr, 0U);\n",
             "registry"),
            ("CreateFileW(nullptr, 0U, 0U, nullptr, 0U, 0U, nullptr);\n",
             "filesystem"),
            ("sqlite3* database;\n", "settings/database"),
            ("GetEnvironmentVariableW(nullptr, nullptr, 0U);\n",
             "environment"),
            ("ShellExecuteW(nullptr, nullptr, nullptr, nullptr, nullptr, 0);\n",
             "command/process"),
            ("WinHttpOpen(nullptr, 0U, nullptr, nullptr, 0U);\n", "network"),
            ("MessageBoxW(nullptr, nullptr, nullptr, 0U);\n", "Qt/UI"),
            ("OutputDebugStringW(nullptr);\n", "logging/printing"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                self.assert_failure(
                    f"Forbidden {surface_name} surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE
                            + hostile_source
                    },
                )

    def test_windows_vault_rejects_raw_error_formatting(self) -> None:
        forbidden_formatters = (
            "FormatMessageW(0U, nullptr, status, 0U, nullptr, 0U, nullptr);\n",
            "std::to_string(status);\n",
            "snprintf(buffer, sizeof(buffer), \"%lu\", status);\n",
        )
        for formatter in forbidden_formatters:
            with self.subTest(formatter=formatter):
                self.assert_failure(
                    "Forbidden raw operating-system error formatting surface",
                    {
                        "src/adapters/github/src/"
                        "windows_credential_manager.cpp":
                            WINDOWS_CREDENTIAL_MANAGER_SOURCE + formatter
                    },
                )

    def test_macos_keychain_source_is_required(self) -> None:
        self.assert_failure(
            "designated macOS Keychain source is required",
            omit_macos_source=True,
        )

    def test_macos_keychain_source_must_be_owned_by_apple_target(self) -> None:
        self.assert_failure(
            "Apple GitHub adapter must compile src/macos_keychain.cpp",
            github_target=GITHUB_TARGET.replace(
                "            src/macos_keychain.cpp\n",
                "",
            ),
        )

    def test_apple_target_requires_native_frameworks(self) -> None:
        hostile_targets = (
            (
                GITHUB_TARGET.replace(
                    "    find_library(BPR_TEST_SECURITY_FRAMEWORK "
                    "Security REQUIRED)\n",
                    "",
                ),
                "must find Security with REQUIRED",
            ),
            (
                GITHUB_TARGET.replace(
                    "    find_library(\n"
                    "        BPR_TEST_CORE_FOUNDATION_FRAMEWORK\n"
                    "        CoreFoundation\n"
                    "        REQUIRED\n"
                    "    )\n",
                    "",
                ),
                "must find CoreFoundation with REQUIRED",
            ),
            (
                GITHUB_TARGET.replace(
                    "            ${BPR_TEST_SECURITY_FRAMEWORK}\n",
                    "",
                ),
                "must link the required Security framework",
            ),
            (
                GITHUB_TARGET.replace(
                    "            ${BPR_TEST_CORE_FOUNDATION_FRAMEWORK}\n",
                    "",
                ),
                "must link the required CoreFoundation framework",
            ),
        )
        for target, expected_message in hostile_targets:
            with self.subTest(expected_message=expected_message):
                self.assert_failure(
                    expected_message,
                    github_target=target,
                )

    def test_macos_keychain_requires_an_apple_target_branch(self) -> None:
        self.assert_failure(
            "must define an APPLE branch",
            github_target=GITHUB_TARGET.replace(APPLE_KEYCHAIN_TARGET, ""),
        )

    def test_selected_macos_keychain_source_is_allowed(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_macos_keychain_requires_the_effect_executor(self) -> None:
        self.assert_failure(
            "requires the host effect executor",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE.replace(
                        "namespace application { class effect_executor; }\n"
                        "application::effect_executor* "
                        "selected_effect_lane = nullptr;\n",
                        "",
                    )
            },
        )

    def test_macos_keychain_rejects_the_decision_executor(self) -> None:
        self.assert_failure(
            "Forbidden serialized decision-executor surface",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE
                    + "application::decision_executor* "
                    "forbidden_decision_lane = nullptr;\n"
            },
        )

    def test_macos_keychain_rejects_backend_switch_attributes(self) -> None:
        forbidden_attributes = (
            "kSecUseDataProtectionKeychain",
            "kSecAttrSynchronizable",
            "kSecAttrAccessGroup",
            "kSecAttrAccessible",
            "kSecAttrAccessibleWhenUnlocked",
        )
        for attribute in forbidden_attributes:
            with self.subTest(attribute=attribute):
                self.assert_failure(
                    "Forbidden Keychain backend-switch surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE
                            + f"const void* forbidden = {attribute};\n"
                    },
                )

    def test_macos_keychain_requires_safe_query_properties(self) -> None:
        required_properties = (
            "kSecClassGenericPassword",
            "kSecAttrService",
            "kSecAttrAccount",
            "kSecUseKeychain",
            "kSecMatchSearchList",
            "kSecUseAuthenticationUI",
            "kSecUseAuthenticationUIFail",
            "kSecReturnData",
            "kSecMatchLimit",
            "kSecMatchLimitOne",
        )
        for query_property in required_properties:
            with self.subTest(query_property=query_property):
                self.assert_failure(
                    "must retain the exact-keychain",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE.replace(
                                f"        {query_property},\n",
                                "",
                            )
                    },
                )

    def test_macos_keychain_rejects_expansive_query_policy(self) -> None:
        forbidden_properties = (
            "kSecClassInternetPassword",
            "kSecClassCertificate",
            "kSecReturnAttributes",
            "kSecReturnRef",
            "kSecReturnPersistentRef",
            "kSecMatchLimitAll",
            "kSecUseAuthenticationUIAllow",
        )
        for query_property in forbidden_properties:
            with self.subTest(query_property=query_property):
                self.assert_failure(
                    "Forbidden Keychain query-policy surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE
                            + f"const void* forbidden = {query_property};\n"
                    },
                )

    def test_macos_keychain_rejects_unselected_keychain_effects(
        self,
    ) -> None:
        self.assert_failure(
            "Keychain effect outside the selected",
            {
                "src/adapters/github/src/macos_keychain.cpp":
                    MACOS_KEYCHAIN_SOURCE
                    + "SecKeychainGetStatus(keychain, &status);\n"
            },
        )

    def test_macos_keychain_rejects_raw_os_error_formatting(self) -> None:
        forbidden_formatters = (
            "SecCopyErrorMessageString(status, nullptr);\n",
            "std::to_string(status);\n",
            "snprintf(buffer, sizeof(buffer), \"%d\", status);\n",
        )
        for formatter in forbidden_formatters:
            with self.subTest(formatter=formatter):
                self.assert_failure(
                    "Forbidden raw operating-system error formatting surface",
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE + formatter
                    },
                )

    def test_macos_keychain_rejects_non_vault_effect_families(self) -> None:
        hostile_surfaces = (
            ("QWidget* widget;\n", "Qt/UI"),
            ("NSURLSession* session;\n", "network"),
            ("sqlite3* database;\n", "settings/database"),
            ("QSettings* settings;\n", "Qt/UI"),
            ("std::filesystem::path fallback;\n", "filesystem"),
            ("std::ofstream plaintext_file;\n", "filesystem"),
            ("write(descriptor, bytes, size);\n", "filesystem"),
            ("auto* value = getenv(\"BPR_TOKEN\");\n", "environment"),
            ("posix_spawn(nullptr, nullptr, nullptr, nullptr, nullptr, "
             "nullptr);\n", "command/process"),
            ("std::cerr << status;\n", "logging/printing"),
        )
        for hostile_source, surface_name in hostile_surfaces:
            with self.subTest(surface_name=surface_name):
                expected = (
                    "posix_spawn ownership violation"
                    if "posix_spawn" in hostile_source
                    else f"Forbidden {surface_name} surface"
                )
                self.assert_failure(
                    expected,
                    {
                        "src/adapters/github/src/macos_keychain.cpp":
                            MACOS_KEYCHAIN_SOURCE + hostile_source
                    },
                )

    def test_native_vault_apis_are_rejected_outside_designated_sources(
        self,
    ) -> None:
        hostile_apis = (
            ("SecItemAdd(query, nullptr);\n", "macOS Keychain API"),
            (
                "SecKeychainAddGenericPassword(nullptr, 0, nullptr);\n",
                "macOS Keychain API",
            ),
            ("CredWriteW(&credential, 0);\n", "Windows Credential Manager API"),
            (
                "secret_password_store_sync(schema, nullptr);\n",
                "Linux Secret Service API",
            ),
            (
                "secret_item_delete_sync(item, nullptr);\n",
                "Linux Secret Service API",
            ),
            (
                'auto service = "org.freedesktop.Secret.Service";\n',
                "Linux Secret Service API",
            ),
        )
        for source, ownership_message in hostile_apis:
            with self.subTest(source=source):
                self.assert_failure(
                    f"{ownership_message} ownership violation",
                    {"src/adapters/github/src/other_vault.cpp": source},
                )

    def test_designated_native_vault_sources_are_allowed(self) -> None:
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_profile_activation_listener_is_exact_and_local(self) -> None:
        path = "src/adapters/sqlite/src/profile_activation.cpp"
        source = (REPOSITORY_ROOT / path).read_text(encoding="utf-8")
        result = self.run_check({path: source})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_failure("Profile activation must use only local Unix sockets",
                            {path: source.replace("AF_UNIX", "AF_INET")})
        self.assert_failure("Forbidden runtime surface", {path: source + "\nlisten(other, 8);\n"})
        self.assert_failure("Forbidden runtime surface",
                            {"src/adapters/sqlite/src/other_listener.cpp": "::listen(listener.Value, 8);\n"})

    def test_adapter_root_must_delegate_target_ownership(self) -> None:
        self.assert_failure(
            "Adapter root must delegate",
            adapter_root="",
        )

    def test_target_ownership_ignores_other_source_trees(self) -> None:
        result = self.run_check({
            ".claude/worktrees/other/src/adapters/github/CMakeLists.txt": GITHUB_TARGET,
            "build/extracted-source/src/adapters/github/CMakeLists.txt": GITHUB_TARGET,
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_target_ownership_still_checks_root_and_nested_product_sources(self) -> None:
        for path in ("CMakeLists.txt", "src/adapters/other/nested/CMakeLists.txt"):
            with self.subTest(path=path):
                self.assert_failure("must be declared only", {path: GITHUB_TARGET})

    def test_github_target_cannot_be_declared_at_adapter_root(self) -> None:
        self.assert_failure(
            "must be declared only",
            adapter_root=(
                ADAPTER_ROOT
                + "add_library(better_pull_review_adapter_github INTERFACE)\n"
            ),
        )


if __name__ == "__main__":
    unittest.main()
