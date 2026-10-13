if(NOT DEFINED BPR_ROOT)
    message(FATAL_ERROR "BPR_ROOT is required")
endif()

set(github_adapter_cmake "${BPR_ROOT}/src/adapters/github/CMakeLists.txt")
set(git_adapter_cmake "${BPR_ROOT}/src/adapters/git/CMakeLists.txt")
set(adapters_cmake "${BPR_ROOT}/src/adapters/CMakeLists.txt")
set(core_cmake "${BPR_ROOT}/src/core/CMakeLists.txt")
set(dependencies_cmake "${BPR_ROOT}/cmake/BprDependencies.cmake")
set(
    github_process_runner_relative
    "src/adapters/github/src/github_cli_process_runner.cpp"
)
set(
    github_process_interface_relative
    "src/adapters/github/src/github_cli_process.hpp"
)
set(
    github_process_runner_interface_relative
    "src/adapters/github/src/github_cli_process_runner.hpp"
)
set(
    github_native_https_relative
    "src/adapters/github/src/native_https_transport.cpp"
)
set(
    github_native_https_interface_relative
    "src/adapters/github/src/native_https_transport.hpp"
)
set(
    github_designated_live_observer_relative
    "src/adapters/github/src/designated_live_public_observer.cpp"
)
set(
    github_designated_live_observer_interface_relative
    "src/adapters/github/src/designated_live_public_observer.hpp"
)
set(
    app_designated_live_supervisor_relative
    "src/app/src/designated_live_macos_supervisor.cpp"
)
set(
    app_designated_live_supervisor_identity_relative
    "src/app/src/designated_live_macos_supervisor_identity.cpp"
)
set(
    app_designated_live_supervisor_launch_relative
    "src/app/src/designated_live_macos_supervisor_launch.cpp"
)
set(
    designated_live_macos_capsule_script_relative
    "scripts/designated_live_macos_capsule.py"
)
set(
    designated_live_stage_zero_preloader_relative
    "cmake/BprDesignatedLiveStageZero.cmake"
)
set(
    designated_live_test_isolation_relative
    "cmake/BprTestIsolation.cmake"
)
set(
    app_entry_point_relative
    "src/app/main.cpp"
)
set(
    app_designated_live_provenance_relative
    "src/app/src/designated_live_macos_provenance.cpp"
)
set(
    github_wire_validation_relative
    "src/adapters/github/src/github_wire_request_validation.cpp"
)
set(
    github_wire_validation_interface_relative
    "src/adapters/github/src/github_wire_request_validation.hpp"
)
set(
    github_cli_transport_relative
    "src/adapters/github/src/gh_cli_transport.cpp"
)
set(
    macos_keychain_relative
    "src/adapters/github/src/macos_keychain.cpp"
)
set(
    windows_credential_manager_relative
    "src/adapters/github/src/windows_credential_manager.cpp"
)
set(
    linux_secret_service_relative
    "src/adapters/github/src/linux_secret_service.cpp"
)
set(
    git_process_runner_relative
    "src/adapters/git/src/installed_git_process.cpp"
)
# [[BPR-1436]]. The QProcess supervision the gh and Git runners share: one
# serialized lane with its quarantine, and the process-group wait, kill,
# resolution, kill-and-reap, background reap and abandonment. It is a
# designated QProcess owner of its own because neither runner may depend on the
# other's adapter.
set(
    process_support_source_relative
    "src/adapters/process_support/src/process_supervision.cpp"
)
set(
    process_support_header_relative
    "src/adapters/process_support/include/bpr/adapters/process_support/process_supervision.hpp"
)
# Core names the safe environment vocabulary in immutable agent profiles. It
# decides which names may cross; it still owns no environment or process API.
set(
    agent_profile_policy_relative
    "src/core/src/agent_profile.cpp"
)
# The third designated process owner, added by BPR-0175. RFC-BPR-0011 launches
# separately installed agent tools, which is a process this product starts and
# does not audit; it therefore needs an owner of its own rather than borrowing
# the Git adapter's. The list is deliberately short and adding to it is
# deliberately a visible act.
set(
    agent_process_runner_relative
    "src/adapters/agent/src/agent_process_launcher.cpp"
)
set(
    agent_unix_supervisor_relative
    "src/adapters/agent/src/unix_agent_supervisor.cpp"
)
# [[BPR-0622]]. The Windows counterpart of the Unix supervisor: it owns the
# child's job from outside it, so it is a designated process owner too.
set(
    agent_windows_supervisor_relative
    "src/adapters/agent/src/windows_agent_supervisor.cpp"
)
set(
    agent_protocol_loop_relative
    "src/adapters/agent/src/agent_protocol_loop.cpp"
)
set(
    agent_windows_process_relative
    "src/adapters/agent/src/windows_agent_process.cpp"
)
set(
    git_process_port_relative
    "src/application/include/bpr/application/git_process_port.hpp"
)
set(
    git_process_factory_relative
    "src/adapters/git/include/bpr/adapters/git/installed_git_process.hpp"
)
if(NOT EXISTS "${github_adapter_cmake}")
    message(FATAL_ERROR "GitHub adapter must own a dedicated CMake target")
endif()
if(NOT EXISTS "${git_adapter_cmake}")
    message(FATAL_ERROR "Git adapter must own a dedicated CMake target")
endif()
if(NOT EXISTS "${dependencies_cmake}")
    message(FATAL_ERROR "The root dependency resolver is required")
endif()
file(READ "${dependencies_cmake}" dependencies_cmake_contents)
foreach(
    pattern
    IN ITEMS
        "function(bpr_configure_dependencies)"
        "bpr_include_builtin_find_module(Threads)"
        "Qt6::"
        "LibSecret::LibSecret"
        "Python3::Interpreter"
        "Git::Git"
        "bpr_acquire_quill()"
        "bpr_require_dependency_target(quill::quill Quill)"
)
    string(FIND "${dependencies_cmake_contents}" "${pattern}" pattern_index)
    if(pattern_index EQUAL -1)
        message(FATAL_ERROR "Root dependency ownership is incomplete: ${pattern}")
    endif()
endforeach()
file(READ "${adapters_cmake}" adapters_cmake_contents)
if(
    NOT adapters_cmake_contents
        MATCHES "add_subdirectory\\([ \t\r\n]*github[ \t\r\n]*\\)"
)
    message(FATAL_ERROR "Adapter root must delegate to the GitHub subdirectory")
endif()
if(
    NOT adapters_cmake_contents
        MATCHES "add_subdirectory\\([ \t\r\n]*git[ \t\r\n]*\\)"
)
    message(FATAL_ERROR "Adapter root must delegate to the Git subdirectory")
endif()

file(READ "${git_adapter_cmake}" git_cmake_contents)
# Read only where it exists. An unconditional requirement here fires before
# every other check on any tree without this adapter — including the synthetic
# trees `architecture_boundary_tests.py` builds to prove the scan detects each
# violation, where it masked all of them. The ownership loop below already
# skips a designated file that is absent, so the check that matters is still
# made wherever the adapter is real.
set(agent_adapter_cmake "${BPR_ROOT}/src/adapters/agent/CMakeLists.txt")
set(agent_cmake_contents "")
if(EXISTS "${agent_adapter_cmake}")
    file(READ "${agent_adapter_cmake}" agent_cmake_contents)
endif()
if(NOT agent_cmake_contents STREQUAL "")
    foreach(
        pattern
        IN ITEMS
            "add_library\\([ \t\r\n]*better_pull_review_adapter_agent([ \t\r\n]|\\))"
            "src/agent_process_launcher\\.cpp"
            "src/agent_protocol_loop\\.cpp"
            "bpr_add_executable\\([ \t\r\n]*tool_better_pull_review_agent_supervisor[ \t\r\n]+src/unix_agent_supervisor\\.cpp[ \t\r\n]*\\)"
            "if\\(WIN32\\)"
            "src/windows_agent_process\\.cpp"
            # [[BPR-0842]] A link names its target: the Windows supervisor
            # links Kernel32 too, and a bare token cannot tell the two apart.
            "target_link_libraries\\([ \t\r\n]*better_pull_review_adapter_agent[ \t\r\n]([^)]*[ \t\r\n])?Kernel32([ \t\r\n]|\\))"
            "if\\(BUILD_TESTING\\)"
            "BPR_BUILD_TESTING=1"
    )
        if(NOT agent_cmake_contents MATCHES "${pattern}")
            message(
                FATAL_ERROR
                "Agent process adapter target contract is incomplete: ${pattern}"
            )
        endif()
    endforeach()
    # The Windows supervisor is a designated owner of the native process and
    # job APIs below; that permission presumes this executable, built from that
    # source, with its own Kernel32 link.
    foreach(
        pattern
        IN ITEMS
            "bpr_add_executable\\([ \t\r\n]*tool_better_pull_review_agent_supervisor[ \t\r\n]+src/windows_agent_supervisor\\.cpp[ \t\r\n]*\\)"
            "target_link_libraries\\([ \t\r\n]*tool_better_pull_review_agent_supervisor[ \t\r\n]([^)]*[ \t\r\n])?Kernel32([ \t\r\n]|\\))"
    )
        if(NOT agent_cmake_contents MATCHES "${pattern}")
            message(
                FATAL_ERROR
                "Windows agent supervisor target contract is incomplete: ${pattern}"
            )
        endif()
    endforeach()
    foreach(
        required_agent_source
        IN ITEMS
            "${agent_process_runner_relative}"
            "${agent_protocol_loop_relative}"
            "${agent_windows_process_relative}"
            "${agent_unix_supervisor_relative}"
            "${agent_windows_supervisor_relative}"
    )
        if(NOT EXISTS "${BPR_ROOT}/${required_agent_source}")
            message(
                FATAL_ERROR
                "The selected agent process source is required: "
                "${required_agent_source}"
            )
        endif()
    endforeach()
    file(
        READ
        "${BPR_ROOT}/${agent_process_runner_relative}"
        agent_process_runner_contents
    )
    file(
        READ
        "${BPR_ROOT}/${agent_protocol_loop_relative}"
        agent_protocol_loop_contents
    )
    if(NOT EXISTS "${BPR_ROOT}/${agent_windows_process_relative}")
        message(
            FATAL_ERROR
            "The designated native Windows agent process source is required: "
            "${agent_windows_process_relative}"
        )
    endif()
    file(
        READ
        "${BPR_ROOT}/${agent_windows_process_relative}"
        agent_windows_process_contents
    )
    foreach(
        required_shared_agent_protocol_pattern
        IN ITEMS
            "agent_protocol_decoder"
            "build_agent_protocol_followup"
            "maximum_agent_stderr_bytes"
            "AppServerPhase"
    )
        if(
            NOT agent_protocol_loop_contents
                MATCHES "${required_shared_agent_protocol_pattern}"
        )
            message(
                FATAL_ERROR
                "Shared agent protocol loop contract is incomplete: "
                "${required_shared_agent_protocol_pattern}"
            )
        endif()
    endforeach()
    foreach(
        agent_protocol_backend_contents
        IN ITEMS
            "${agent_process_runner_contents}"
            "${agent_windows_process_contents}"
    )
        if(
            NOT agent_protocol_backend_contents
                MATCHES "agent_protocol_loop\\.hpp"
            OR NOT agent_protocol_backend_contents
                MATCHES "run_agent_protocol_loop"
        )
            message(
                FATAL_ERROR
                "Agent process backend must select the shared protocol loop"
            )
        endif()
        foreach(
            duplicated_agent_protocol_pattern
            IN ITEMS
                "agent_protocol_decoder"
                "build_agent_protocol_followup"
                "maximum_agent_stderr_bytes"
                "[Aa]ppServerPhase"
        )
            if(
                agent_protocol_backend_contents
                    MATCHES "${duplicated_agent_protocol_pattern}"
            )
                message(
                    FATAL_ERROR
                    "Agent process backend duplicates shared protocol logic: "
                    "${duplicated_agent_protocol_pattern}"
                )
            endif()
        endforeach()
    endforeach()
    foreach(
        required_native_agent_process_pattern
        IN ITEMS
            "CreateProcessW"
            "CREATE_SUSPENDED"
            "CREATE_UNICODE_ENVIRONMENT"
            "EXTENDED_STARTUPINFO_PRESENT"
            "PROC_THREAD_ATTRIBUTE_HANDLE_LIST"
            "STARTF_USESTDHANDLES"
            "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE"
            "AssignProcessToJobObject"
            "ResumeThread"
            "TerminateJobObject"
            "CancelIoEx"
            "CancelSynchronousIo"
            "QueryInformationJobObject"
            "BPR_BUILD_TESTING"
            "ForceJobAssignmentFailure"
    )
        if(
            NOT agent_windows_process_contents
                MATCHES "${required_native_agent_process_pattern}"
        )
            message(
                FATAL_ERROR
                "Native Windows agent process contract is incomplete: "
                "${required_native_agent_process_pattern}"
            )
        endif()
    endforeach()
    foreach(
        forbidden_native_agent_process_pattern
        IN ITEMS
            "QProcess"
            "CreateProcessA"
            "CreateProcess(AsUser|WithLogon|WithToken)[AW]?"
            "ShellExecute(Ex)?[AW]?"
            "WinExec"
            "CREATE_BREAKAWAY_FROM_JOB"
            "JOB_OBJECT_LIMIT_(BREAKAWAY_OK|SILENT_BREAKAWAY_OK)"
            "CREATE_(NEW_PROCESS_GROUP|NEW_CONSOLE)"
            "DETACHED_PROCESS"
            "GetEnvironment(Strings|Variable)[AW]?"
            "QProcessEnvironment"
            "(/bin/(ba|z|k|c)?sh|cmd(\\.exe)?|powershell(\\.exe)?|pwsh(\\.exe)?)"
            "std::(cout|cerr|clog)"
    )
        if(
            agent_windows_process_contents
                MATCHES "${forbidden_native_agent_process_pattern}"
        )
            message(
                FATAL_ERROR
                "Forbidden native Windows agent process surface: "
                "${forbidden_native_agent_process_pattern}"
            )
        endif()
    endforeach()
endif()
set(
    git_target_patterns
    "add_library\\([ \t\r\n]*better_pull_review_adapter_git([ \t\r\n]|\\))"
    "add_library\\([ \t\r\n]*BetterPullReview::AdapterGit[ \t\r\n]+ALIAS[ \t\r\n]+better_pull_review_adapter_git[ \t\r\n]*\\)"
    "src/installed_git_process\\.cpp"
    "BetterPullReview::Application"
    "Qt6::Core"
    "Threads::Threads"
)
foreach(pattern IN LISTS git_target_patterns)
    if(NOT git_cmake_contents MATCHES "${pattern}")
        message(
            FATAL_ERROR
            "Installed-Git adapter target contract is incomplete: ${pattern}"
        )
    endif()
endforeach()
if(git_cmake_contents MATCHES "Qt6::Network")
    message(FATAL_ERROR "Installed-Git adapter must not link Qt Network")
endif()
if(
    git_cmake_contents MATCHES "BPR_[A-Z0-9_]*TEST"
    OR git_cmake_contents MATCHES "BPR_BUILD_TESTING"
)
    message(
        FATAL_ERROR
        "Installed-Git adapter must not compile a test-only production seam"
    )
endif()
foreach(
    required_git_file_relative
    IN ITEMS
        "${git_process_runner_relative}"
        "${git_process_port_relative}"
        "${git_process_factory_relative}"
)
    if(NOT EXISTS "${BPR_ROOT}/${required_git_file_relative}")
        message(
            FATAL_ERROR
            "Required installed-Git boundary file is missing: "
            "${required_git_file_relative}"
        )
    endif()
endforeach()

file(READ "${github_adapter_cmake}" github_cmake_contents)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    github_cmake_compact
    "${github_cmake_contents}"
)
set(
    github_target_patterns
    "add_library\\([ \t\r\n]*better_pull_review_adapter_github([ \t\r\n]|\\))"
    "add_library\\([ \t\r\n]*BetterPullReview::AdapterGitHub[ \t\r\n]+ALIAS[ \t\r\n]+better_pull_review_adapter_github[ \t\r\n]*\\)"
    "target_link_libraries\\([ \t\r\n]*better_pull_review_adapter_github"
    "BetterPullReview::Application"
)
foreach(pattern IN LISTS github_target_patterns)
    if(NOT github_cmake_contents MATCHES "${pattern}")
        message(
            FATAL_ERROR
            "GitHub adapter target contract is incomplete: ${pattern}"
        )
    endif()
endforeach()

if(
    NOT github_cmake_contents
        MATCHES
            "target_link_libraries[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*PRIVATE[^\\)]*Threads::Threads[^\\)]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub adapter must link Threads::Threads explicitly and privately"
    )
endif()

foreach(
    required_github_host_file_relative
    IN ITEMS
        "${github_process_interface_relative}"
        "${github_process_runner_interface_relative}"
        "${github_process_runner_relative}"
        "${github_cli_transport_relative}"
        "${github_native_https_interface_relative}"
        "${github_native_https_relative}"
        "${github_wire_validation_interface_relative}"
        "${github_wire_validation_relative}"
)
    get_filename_component(
        required_github_host_file
        "${BPR_ROOT}/${required_github_host_file_relative}"
        ABSOLUTE
    )
    if(NOT EXISTS "${required_github_host_file}")
        message(
            FATAL_ERROR
            "The designated GitHub host adapter file is required: "
            "${required_github_host_file_relative}"
        )
    endif()
endforeach()

function(require_github_target_source required_source_relative)
    if(
        NOT github_cmake_contents
            MATCHES
                "(add_library|target_sources)[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*${required_source_relative}"
    )
        message(
            FATAL_ERROR
            "GitHub host adapter source must be explicitly owned by "
            "better_pull_review_adapter_github: ${required_source_relative}"
        )
    endif()
endfunction()

require_github_target_source("src/github_cli_process_runner\\.cpp")
require_github_target_source("src/gh_cli_transport\\.cpp")
require_github_target_source("src/github_wire_request_validation\\.cpp")
require_github_target_source("src/native_https_transport\\.cpp")

if(
    NOT github_cmake_contents
        MATCHES
            "target_link_libraries[ \t\r\n]*\\([ \t\r\n]*better_pull_review_adapter_github[^\\)]*PRIVATE[^\\)]*Qt6::Network[^\\)]*\\)"
)
    message(
        FATAL_ERROR
        "GitHub native HTTPS must link Qt6::Network privately"
    )
endif()
if(
    NOT github_cmake_compact
        MATCHES
            "if\\(BUILD_TESTING\\) target_compile_definitions\\( better_pull_review_adapter_github PRIVATE BPR_NATIVE_HTTPS_TESTING=1 \\) endif\\(\\)"
)
    message(
        FATAL_ERROR
        "Native HTTPS fixture seams must compile privately and only in "
        "test-enabled builds"
    )
endif()

get_filename_component(
    macos_keychain_file
    "${BPR_ROOT}/${macos_keychain_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${macos_keychain_file}")
    message(
        FATAL_ERROR
        "The designated macOS Keychain source is required: "
        "${macos_keychain_relative}"
    )
endif()

get_filename_component(
    windows_credential_manager_file
    "${BPR_ROOT}/${windows_credential_manager_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${windows_credential_manager_file}")
    message(
        FATAL_ERROR
        "The designated Windows Credential Manager source is required: "
        "${windows_credential_manager_relative}"
    )
endif()

get_filename_component(
    linux_secret_service_file
    "${BPR_ROOT}/${linux_secret_service_relative}"
    ABSOLUTE
)
if(NOT EXISTS "${linux_secret_service_file}")
    message(
        FATAL_ERROR
        "The designated Linux Secret Service source is required: "
        "${linux_secret_service_relative}"
    )
endif()

file(STRINGS "${github_adapter_cmake}" github_cmake_lines)
set(github_apple_branch_contents "")
set(in_github_apple_branch FALSE)
set(github_apple_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_apple_branch FALSE)
    if(
        NOT in_github_apple_branch
        AND github_cmake_line
            MATCHES "^[ \t]*if[ \t]*\\([ \t]*APPLE[ \t]*\\)"
    )
        set(in_github_apple_branch TRUE)
        set(started_github_apple_branch TRUE)
        set(github_apple_branch_depth 1)
    endif()

    if(NOT in_github_apple_branch)
        continue()
    endif()

    string(APPEND github_apple_branch_contents "${github_cmake_line}\n")
    if(
        NOT started_github_apple_branch
        AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
    )
        math(EXPR github_apple_branch_depth "${github_apple_branch_depth} + 1")
    endif()
    if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
        math(EXPR github_apple_branch_depth "${github_apple_branch_depth} - 1")
        if(github_apple_branch_depth EQUAL 0)
            set(in_github_apple_branch FALSE)
        endif()
    endif()
endforeach()

if(github_apple_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define an APPLE branch for Keychain"
    )
endif()
if(
    NOT github_apple_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*macos_keychain\\.cpp"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must compile src/macos_keychain.cpp"
    )
endif()

string(
    REGEX MATCH
    "find_library[ \t\r\n]*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]+Security[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
    github_security_find
    "${github_apple_branch_contents}"
)
set(github_security_framework_variable "${CMAKE_MATCH_1}")
if(github_security_find STREQUAL "")
    message(
        FATAL_ERROR
        "Apple GitHub adapter must find Security with REQUIRED"
    )
endif()

string(
    REGEX MATCH
    "find_library[ \t\r\n]*\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]+CoreFoundation[ \t\r\n]+REQUIRED[ \t\r\n]*\\)"
    github_core_foundation_find
    "${github_apple_branch_contents}"
)
set(github_core_foundation_framework_variable "${CMAKE_MATCH_1}")
if(github_core_foundation_find STREQUAL "")
    message(
        FATAL_ERROR
        "Apple GitHub adapter must find CoreFoundation with REQUIRED"
    )
endif()

string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_apple_target_links
    "${github_apple_branch_contents}"
)
if(
    github_apple_target_links STREQUAL ""
    OR NOT github_apple_target_links
        MATCHES "${github_security_framework_variable}"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must link the required Security framework"
    )
endif()
if(
    github_apple_target_links STREQUAL ""
    OR NOT github_apple_target_links
        MATCHES "${github_core_foundation_framework_variable}"
)
    message(
        FATAL_ERROR
        "Apple GitHub adapter must link the required CoreFoundation framework"
    )
endif()

set(github_win32_branch_contents "")
set(in_github_win32_branch FALSE)
set(github_win32_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_win32_branch FALSE)
    if(
        NOT in_github_win32_branch
        AND github_cmake_line
            MATCHES "^[ \t]*if[ \t]*\\([ \t]*WIN32[ \t]*\\)"
    )
        set(in_github_win32_branch TRUE)
        set(started_github_win32_branch TRUE)
        set(github_win32_branch_depth 1)
    endif()

    if(NOT in_github_win32_branch)
        continue()
    endif()

    string(APPEND github_win32_branch_contents "${github_cmake_line}\n")
    if(
        NOT started_github_win32_branch
        AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
    )
        math(EXPR github_win32_branch_depth "${github_win32_branch_depth} + 1")
    endif()
    if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
        math(EXPR github_win32_branch_depth "${github_win32_branch_depth} - 1")
        if(github_win32_branch_depth EQUAL 0)
            set(in_github_win32_branch FALSE)
        endif()
    endif()
endforeach()

if(github_win32_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define a WIN32 branch for "
        "Windows Credential Manager"
    )
endif()
if(
    NOT github_win32_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*windows_credential_manager\\.cpp"
)
    message(
        FATAL_ERROR
        "Windows GitHub adapter must compile "
        "src/windows_credential_manager.cpp"
    )
endif()

string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_win32_target_links
    "${github_win32_branch_contents}"
)
if(
    github_win32_target_links STREQUAL ""
    OR NOT github_win32_target_links
        MATCHES
            "(^|[ \t\r\n])Advapi32([ \t\r\n]|\\))"
)
    message(
        FATAL_ERROR
        "Windows GitHub adapter must link Advapi32"
    )
endif()

set(github_linux_branch_contents "")
set(github_non_linux_branch_contents "")
set(in_github_linux_branch FALSE)
set(github_linux_branch_depth 0)
foreach(github_cmake_line IN LISTS github_cmake_lines)
    set(started_github_linux_branch FALSE)
    if(
        NOT in_github_linux_branch
        AND github_cmake_line
            MATCHES
                "^[ \t]*if[ \t]*\\([ \t]*CMAKE_SYSTEM_NAME[ \t]+STREQUAL[ \t]+\"Linux\"[ \t]*\\)"
    )
        set(in_github_linux_branch TRUE)
        set(started_github_linux_branch TRUE)
        set(github_linux_branch_depth 1)
    endif()

    if(in_github_linux_branch)
        string(APPEND github_linux_branch_contents "${github_cmake_line}\n")
        if(
            NOT started_github_linux_branch
            AND github_cmake_line MATCHES "^[ \t]*if[ \t]*\\("
        )
            math(
                EXPR
                github_linux_branch_depth
                "${github_linux_branch_depth} + 1"
            )
        endif()
        if(github_cmake_line MATCHES "^[ \t]*endif[ \t]*\\(")
            math(
                EXPR
                github_linux_branch_depth
                "${github_linux_branch_depth} - 1"
            )
            if(github_linux_branch_depth EQUAL 0)
                set(in_github_linux_branch FALSE)
            endif()
        endif()
        continue()
    endif()

    string(APPEND github_non_linux_branch_contents "${github_cmake_line}\n")
endforeach()

if(github_linux_branch_contents STREQUAL "")
    message(
        FATAL_ERROR
        "GitHub adapter CMake must define an exact Linux branch for "
        "Secret Service"
    )
endif()
if(
    NOT github_linux_branch_contents
        MATCHES
            "target_sources[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*linux_secret_service\\.cpp"
)
    message(
        FATAL_ERROR
        "Linux GitHub adapter must compile src/linux_secret_service.cpp"
    )
endif()
string(
    REGEX MATCH
    "target_link_libraries[ \t\r\n]*\\([^\\)]*better_pull_review_adapter_github[^\\)]*\\)"
    github_linux_target_links
    "${github_linux_branch_contents}"
)
if(
    github_linux_target_links STREQUAL ""
    OR NOT github_linux_target_links
        MATCHES "LibSecret::LibSecret"
)
    message(
        FATAL_ERROR
        "Linux GitHub adapter must link the imported libsecret target"
    )
endif()

if(
    github_non_linux_branch_contents
        MATCHES
            "(linux_secret_service\\.cpp|LibSecret::LibSecret)"
)
    message(
        FATAL_ERROR
        "Linux Secret Service source and dependency must remain Linux-only"
    )
endif()

file(
    GLOB_RECURSE project_cmake_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/*/CMakeLists.txt"
)
# Recursing from the root CMakeLists pattern also visits build trees and
# nested worktrees. Inspect the one root file and the product sources only.
list(APPEND project_cmake_files "${BPR_ROOT}/CMakeLists.txt")
foreach(project_cmake_file IN LISTS project_cmake_files)
    if(project_cmake_file STREQUAL github_adapter_cmake)
        continue()
    endif()

    file(READ "${project_cmake_file}" contents)
    if(
        contents
            MATCHES
                "add_library\\([ \t\r\n]*(better_pull_review_adapter_github|BetterPullReview::AdapterGitHub)([ \t\r\n]|\\))"
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${project_cmake_file}")
        message(
            FATAL_ERROR
            "GitHub adapter target must be declared only in "
            "src/adapters/github/CMakeLists.txt, not ${relative_file}"
        )
    endif()
    if(
        contents
            MATCHES
                "linux_secret_service\\.cpp"
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${project_cmake_file}")
        message(
            FATAL_ERROR
            "Linux Secret Service build ownership must remain in "
            "src/adapters/github/CMakeLists.txt, not ${relative_file}"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE effect_free_region_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/core/*.m"
    "${BPR_ROOT}/src/core/*.mm"
    "${BPR_ROOT}/src/application/*.c"
    "${BPR_ROOT}/src/application/*.cc"
    "${BPR_ROOT}/src/application/*.cpp"
    "${BPR_ROOT}/src/application/*.cxx"
    "${BPR_ROOT}/src/application/*.h"
    "${BPR_ROOT}/src/application/*.hh"
    "${BPR_ROOT}/src/application/*.hpp"
    "${BPR_ROOT}/src/application/*.hxx"
    "${BPR_ROOT}/src/application/*.m"
    "${BPR_ROOT}/src/application/*.mm"
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
set(
    effect_free_region_patterns
    "QProcess"
    "QNetwork[A-Za-z0-9_]*"
    "QJson[A-Za-z0-9_]*"
    "#[ \t]*include[ \t]*[<\"](curl|nlohmann|rapidjson|boost/json)"
)
foreach(effect_free_file IN LISTS effect_free_region_files)
    file(READ "${effect_free_file}" contents)
    foreach(pattern IN LISTS effect_free_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${effect_free_file}")
            message(
                FATAL_ERROR
                "Core/application/Qt effect-boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE standard_cpp_region_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/application/*.c"
    "${BPR_ROOT}/src/application/*.cc"
    "${BPR_ROOT}/src/application/*.cpp"
    "${BPR_ROOT}/src/application/*.cxx"
    "${BPR_ROOT}/src/application/*.h"
    "${BPR_ROOT}/src/application/*.hh"
    "${BPR_ROOT}/src/application/*.hpp"
    "${BPR_ROOT}/src/application/*.hxx"
)
set(
    standard_cpp_region_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "#[ \t]*include[ \t]*[<\"]Qt"
    "QString"
    "QByteArray"
    "QVariant"
    "QUrl"
    "(adapters/github|github_read_contract)"
    "api\\.github\\.com"
    "/api/v3"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "node_id[^\r\n]*:"
)
foreach(standard_cpp_file IN LISTS standard_cpp_region_files)
    file(READ "${standard_cpp_file}" contents)
    foreach(pattern IN LISTS standard_cpp_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${standard_cpp_file}")
            message(
                FATAL_ERROR
                "Core/application standard-C++ boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

set(
    worker_executor_files
    "${BPR_ROOT}/src/app/include/bpr/app/worker_executor.hpp"
    "${BPR_ROOT}/src/app/src/worker_executor.cpp"
)
set(
    worker_executor_forbidden_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "#[ \t]*include[ \t]*[<\"][^>\"]*adapters/(github|git|sqlite|ai)"
    "(^|[^A-Za-z0-9_])[A-Za-z0-9_]+_(provider|transport|profile|vault|credential_manager|secret_service|keychain)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(github_credential_[A-Za-z0-9_]*|git_process_[A-Za-z0-9_]*|sqlite3)([^A-Za-z0-9_]|$)"
)
foreach(worker_executor_file IN LISTS worker_executor_files)
    if(NOT EXISTS "${worker_executor_file}")
        continue()
    endif()

    file(READ "${worker_executor_file}" contents)
    foreach(pattern IN LISTS worker_executor_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${worker_executor_file}")
            message(
                FATAL_ERROR
                "Worker executor Qt/provider boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE qt_github_projection_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
set(
    qt_github_projection_forbidden_patterns
    "QNetwork[A-Za-z0-9_]*"
    "QProcess[A-Za-z0-9_]*"
    "QJson[A-Za-z0-9_]*"
    "QUrl"
    "(^|[^A-Za-z0-9_:])((bpr::)?adapters::github)([^A-Za-z0-9_]|$)"
    "#[ \t]*include[ \t]*[<\"][^>\"]*adapters/github"
    "#[ \t]*include[ \t]*[<\"][^>\"]*(gh_cli_transport|github_(capability_probe|cli_process|host_profile|paged_read|provider|read_contract|wire_request_validation)|native_https_transport|macos_keychain|windows_credential_manager|linux_secret_service)\\.hpp[>\"]"
    "(^|[^A-Za-z0-9_])(validated_github_host_profile|github_host_profile_candidate|native_https_transport|gh_cli_transport|github_cli_process[A-Za-z0-9_]*|github_wire_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(capability_probe|host_profile|paged_read|provider|read_contract)[A-Za-z0-9_]*([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(credential_[A-Za-z0-9_]*|provider_[A-Za-z0-9_]*|account_transport[A-Za-z0-9_]*|authentication_transport|capability_(cache|port|request|success|error|cancelled|completion|fact|probe)[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_(profile|viewer|pull_request)_(request|success|error|cancelled|completion|port|submission|authentication)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])github_([A-Za-z0-9_]+_record|pull_request_snapshot|session_read_model|read_limits|profile_authority|read_shape|bootstrap_credential)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(provider_(request|response|record|completion|transport|snapshot)|(credential|Credential)(Material|Bytes|Payload|Value|Vault|Token|Secret|Input|Store|Retrieve|Delete))([^A-Za-z0-9_]|$)"
    "https?://"
    "(api\\.github\\.com|/api/v3|/api/graphql)"
    "(^|[^A-Za-z0-9_])(URL|Url|URI|Uri|Endpoint|RestRoot|GraphqlRoot|RestRoute|GraphqlRoute)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(Authorization|BearerToken|AccessToken|AuthToken|TokenBytes)([^A-Za-z0-9_]|$)"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "(^|[^A-Za-z0-9_])(GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN)([^A-Za-z0-9_]|$)"
)
file(
    GLOB_RECURSE provider_free_projection_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/core/*.c"
    "${BPR_ROOT}/src/core/*.cc"
    "${BPR_ROOT}/src/core/*.cpp"
    "${BPR_ROOT}/src/core/*.cxx"
    "${BPR_ROOT}/src/core/*.h"
    "${BPR_ROOT}/src/core/*.hh"
    "${BPR_ROOT}/src/core/*.hpp"
    "${BPR_ROOT}/src/core/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
# Markdown's private document overrides the mandatory Qt signature to deny all
# resources. Strip only this exact inert implementation; every other URL-bearing
# use and every effect pattern remains subject to the ordinary boundary checks.
function(strip_markdown_resource_denial source_file)
    if(source_file STREQUAL "${BPR_ROOT}/src/adapters/qt/src/markdown_document.hpp")
        set(denial_pattern "auto loadResource\\(int, const QUrl&\\) -> QVariant override[ \t\r\n]*\\{[ \t\r\n]*return QByteArray\\{ \\};[ \t\r\n]*\\}")
        if(NOT contents MATCHES "${denial_pattern}")
            message(FATAL_ERROR "Markdown document resource denial must remain unconditional")
        endif()
        string(REGEX REPLACE "${denial_pattern}" "" contents "${contents}")
        set(contents "${contents}" PARENT_SCOPE)
    endif()
endfunction()

set(
    provider_free_projection_patterns
    "QUrl"
    "https?://"
    "Authorization[ \t]*:"
    "Bearer[ \t]+"
    "bpr/application/github_(read_model|provider_port)"
    "(adapters/github|github_read_contract)"
    "node_id[^\r\n]*:"
)
foreach(provider_free_file IN LISTS provider_free_projection_files)
    file(READ "${provider_free_file}" contents)
    strip_markdown_resource_denial("${provider_free_file}")
    foreach(pattern IN LISTS provider_free_projection_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${provider_free_file}")
            message(
                FATAL_ERROR
                "Provider data crossed the core/Qt boundary in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

set(
    credential_free_region_patterns
    "github_credential_"
)
foreach(credential_free_file IN LISTS provider_free_projection_files)
    file(READ "${credential_free_file}" contents)
    foreach(pattern IN LISTS credential_free_region_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${credential_free_file}")
            message(
                FATAL_ERROR
                "Credential-vault capability crossed the core/Qt boundary in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    GLOB_RECURSE qt_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/adapters/qt/*.cc"
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.cxx"
    "${BPR_ROOT}/src/adapters/qt/*.h"
    "${BPR_ROOT}/src/adapters/qt/*.hh"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.hxx"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
)
set(
    qt_forbidden_patterns
    "QNetwork"
    "QProcess"
    "QTcp"
    "QUdp"
    "QLocalServer"
    "QPluginLoader"
    "QLibrary"
    "QDesktopServices"
    "QUrl"
    "sqlite3"
    "bpr/adapters/(github|git|sqlite|ai)"
    "bpr/application/github_(read_model|provider_port)"
    "(REST|GraphQL|HTTP)[A-Za-z_]*(Response|Request)"
)

foreach(qt_file IN LISTS qt_files)
    file(READ "${qt_file}" contents)
    strip_markdown_resource_denial("${qt_file}")
    foreach(pattern IN LISTS qt_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${qt_file}")
            message(
                FATAL_ERROR
                "Qt adapter effect-boundary violation in ${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

# [[BPR-1354]]. Every tooltip sits next to its control, through the one
# anchored placement in src/adapters/qt/src/anchored_tooltip.cpp, which answers
# every widget's tooltip event once the application object exists.
# `QToolTip::showText` puts its window at the pointer plus the cursor's height,
# which the owner rejected three times ([[BPR-1083]] v004, [[BPR-1328]], and
# BPR-1354: "bug: Why is this damned tooltip far away again. This is a
# consistent problem."), so no Qt source names it. A painted control shows its
# tooltip with show_anchored_tooltip or names it with anchor_tooltips.
file(
    GLOB_RECURSE tooltip_placement_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/adapters/qt/*.cpp"
    "${BPR_ROOT}/src/adapters/qt/*.hpp"
    "${BPR_ROOT}/src/adapters/qt/*.mm"
    "${BPR_ROOT}/src/app/*.cpp"
    "${BPR_ROOT}/src/app/*.hpp"
    "${BPR_ROOT}/src/app/*.mm"
)
foreach(tooltip_file IN LISTS tooltip_placement_files)
    file(READ "${tooltip_file}" contents)
    if(contents MATCHES "QToolTip::showText")
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${tooltip_file}")
        message(
            FATAL_ERROR
            "Tooltip placement violation in ${relative_file}: QToolTip::showText "
            "places a tooltip at the pointer, away from its control; use "
            "show_anchored_tooltip or anchor_tooltips (BPR-1354)"
        )
    endif()
endforeach()

# The single test-only public identity reader is deliberately outside the
# product adapter graph. Keep that narrow allowance visible: no other network
# isolation test source may acquire Qt/network authority, and this source may
# issue only its one unauthenticated hard-coded GET.
set(
    designated_live_test_network_runner_relative
    "tests/network_isolation/designated_live_mutation_runner.cpp"
)
set(
    designated_live_test_network_runner
    "${BPR_ROOT}/${designated_live_test_network_runner_relative}"
)
file(
    GLOB_RECURSE test_network_isolation_cpp_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/tests/network_isolation/*.cc"
    "${BPR_ROOT}/tests/network_isolation/*.cpp"
    "${BPR_ROOT}/tests/network_isolation/*.cxx"
    "${BPR_ROOT}/tests/network_isolation/*.h"
    "${BPR_ROOT}/tests/network_isolation/*.hpp"
    "${BPR_ROOT}/tests/network_isolation/*.hxx"
)
set(test_network_effect_files "")
foreach(test_network_isolation_cpp_file IN LISTS test_network_isolation_cpp_files)
    file(READ "${test_network_isolation_cpp_file}" contents)
    if(
        contents
            MATCHES
                "(QNetwork[A-Za-z0-9_]*|#[ \t]*include[ \t]*[<\"]QtNetwork|https?://)"
    )
        file(
            RELATIVE_PATH
            relative_file
            "${BPR_ROOT}"
            "${test_network_isolation_cpp_file}"
        )
        list(APPEND test_network_effect_files "${relative_file}")
    endif()
endforeach()
if(test_network_effect_files)
    list(REMOVE_DUPLICATES test_network_effect_files)
    list(SORT test_network_effect_files)
    if(
        NOT test_network_effect_files
            STREQUAL designated_live_test_network_runner_relative
    )
        message(
            FATAL_ERROR
            "Test-only network effect ownership violation: "
            "${test_network_effect_files}; expected only "
            "${designated_live_test_network_runner_relative}"
        )
    endif()

    file(READ "${designated_live_test_network_runner}" designated_live_test_network_runner_contents)
    foreach(
        required_surface
        IN ITEMS
            "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/2"
            "QNetworkAccessManager"
            "QNetworkProxy::NoProxy"
            "QNetworkRequest::ManualRedirectPolicy"
            "manager.get(request)"
    )
        string(
            FIND
            "${designated_live_test_network_runner_contents}"
            "${required_surface}"
            required_surface_index
        )
        if(required_surface_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live test reader lacks required fixed surface: "
                "${required_surface}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "[.]get[ \t\r\n]*[(]"
        designated_live_test_get_calls
        "${designated_live_test_network_runner_contents}"
    )
    list(LENGTH designated_live_test_get_calls designated_live_test_get_call_count)
    if(NOT designated_live_test_get_call_count EQUAL 1)
        message(
            FATAL_ERROR
            "Designated live test reader must contain exactly one GET call"
        )
    endif()
    string(
        REPLACE
        "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/2"
        ""
        designated_live_test_network_runner_without_endpoint
        "${designated_live_test_network_runner_contents}"
    )
    if(
        designated_live_test_network_runner_without_endpoint
            MATCHES
                "(https?://|Authorization|QNetworkProxyFactory|systemProxyForQuery|QNetworkRequest::setUrl|QUrl::fromUserInput|[.](post|put|deleteResource|sendCustomRequest|head|connectToHost)[ \t\r\n]*[(]|QTcpSocket|QUdpSocket|QWebSocket|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*|qEnvironmentVariable|QProcessEnvironment::systemEnvironment|(^|[^A-Za-z0-9_])(getenv|setenv|putenv)[ \t\r\n]*[(])"
    )
        message(
            FATAL_ERROR
            "Designated live test reader contains forbidden alternate network, credential, proxy, or environment authority"
        )
    endif()
endif()

# The forked canary is not a general host. Lock every first-party byte that can
# contribute to its translation units, then bind approval to one deterministic
# digest over those four independently locked hashes.
set(
    designated_live_canary_surface_files
    designated_live_mutation_canary.cpp
    designated_live_mutation_canary.hpp
    designated_live_mutation_child.cpp
    designated_live_mutation_child.hpp
)
set(
    designated_live_canary_surface_expected_hashes
    e0de0ae16578b46f208f95c4315b2f0ed9f9301100d4cd6404e6f5a5da720879
    9eebcf65515bccd48204f1c8feefe7bb62cb15c5613d7c250dbbc2a1e0118eb6
    2cab794cd751b7ad775942073dfc1ea21cfaf604a213c8965aa1f4be2dfdbf76
    e7247b22e279b783da025537a642e0bea4dd42cc516e0cef138f758f6bbf2292
)
set(
    designated_live_test_canary
    "${BPR_ROOT}/tests/network_isolation/designated_live_mutation_canary.cpp"
)
if(EXISTS "${designated_live_test_canary}")
    set(designated_live_canary_surface_material "")
    list(LENGTH designated_live_canary_surface_files designated_live_surface_file_count)
    math(EXPR designated_live_surface_last_index "${designated_live_surface_file_count} - 1")
    foreach(index RANGE 0 ${designated_live_surface_last_index})
        list(GET designated_live_canary_surface_files ${index} surface_file_name)
        list(GET designated_live_canary_surface_expected_hashes ${index} expected_surface_hash)
        set(
            surface_file
            "${BPR_ROOT}/tests/network_isolation/${surface_file_name}"
        )
        if(NOT EXISTS "${surface_file}")
            message(
                FATAL_ERROR
                "Designated live canary surface is incomplete: ${surface_file_name}"
            )
        endif()
        file(SHA256 "${surface_file}" actual_surface_hash)
        if(NOT actual_surface_hash STREQUAL expected_surface_hash)
            message(
                FATAL_ERROR
                "Designated live canary surface file is not exact: ${surface_file_name}"
            )
        endif()
        string(
            APPEND
            designated_live_canary_surface_material
            "${surface_file_name}=${actual_surface_hash}\n"
        )
    endforeach()
    string(
        SHA256
        designated_live_canary_surface_sha256
        "${designated_live_canary_surface_material}"
    )
    if(
        NOT designated_live_canary_surface_sha256
            STREQUAL
                "fb77b7ca2a80bea0c584c23dee5c466de0c8bc26c12dee515cf2dc167a05294c"
    )
        message(FATAL_ERROR "Designated live combined canary surface digest drifted")
    endif()

    set(
        designated_live_test_canary_header
        "${BPR_ROOT}/tests/network_isolation/designated_live_mutation_canary.hpp"
    )
    set(
        designated_live_test_child
        "${BPR_ROOT}/tests/network_isolation/designated_live_mutation_child.cpp"
    )
    set(
        designated_live_test_child_header
        "${BPR_ROOT}/tests/network_isolation/designated_live_mutation_child.hpp"
    )
    file(READ "${designated_live_test_canary}" designated_live_test_canary_contents)
    file(READ "${designated_live_test_canary_header}" designated_live_test_canary_header_contents)
    file(READ "${designated_live_test_child}" designated_live_test_child_contents)
    file(READ "${designated_live_test_child_header}" designated_live_test_child_header_contents)

    if(
        designated_live_test_canary_contents
            MATCHES
                "(std::system|(^|[^A-Za-z0-9_])(system|popen|fork|exec[a-zA-Z0-9_]*|posix_spawn[a-zA-Z0-9_]*|spawn[a-zA-Z0-9_]*|socket|curl|fopen|open|unlink|remove|rename|link|mkdir)[ \\t\\r\\n]*[(]|QProcess|QNetwork|QTcp|QUdp|QUrl|std::ofstream|https?://)"
    )
        message(
            FATAL_ERROR
            "Designated live canary contains forbidden process, network, or file-write authority"
        )
    endif()
    foreach(
        header_contents_variable
        IN ITEMS
            designated_live_test_canary_header_contents
            designated_live_test_child_header_contents
    )
        if(
            "${${header_contents_variable}}"
                MATCHES
                    "(QProcess|QNetwork|QTcp|QUdp|QUrl|std::(string[ \\t<*&]|vector|map|function|thread)|(^|[^A-Za-z0-9_])(system|popen|fork|exec[a-zA-Z0-9_]*|posix_spawn[a-zA-Z0-9_]*|spawn[a-zA-Z0-9_]*|socket|curl|fopen|open|malloc|calloc|realloc|free|new)[ \\t\\r\\n]*[(]|https?://|(^|[\\r\\n])[ \\t]*(inline[ \\t]+)?static[^;\\r\\n]*=)"
        )
            message(
                FATAL_ERROR
                "Designated live canary header contains forbidden static initialization, process, network, or allocation authority"
            )
        endif()
    endforeach()

    string(
        REGEX MATCHALL
        "#[ \\t]*include[ \\t]*\"designated_live_mutation_[A-Za-z0-9_]+[.]hpp\""
        designated_live_child_first_party_includes
        "${designated_live_test_child_contents}"
    )
    list(SORT designated_live_child_first_party_includes)
    set(
        designated_live_expected_child_first_party_includes
        "#include \"designated_live_mutation_canary.hpp\""
        "#include \"designated_live_mutation_child.hpp\""
    )
    list(SORT designated_live_expected_child_first_party_includes)
    if(
        NOT designated_live_child_first_party_includes
            STREQUAL designated_live_expected_child_first_party_includes
    )
        message(
            FATAL_ERROR
            "Designated live fork child must include only its two locked first-party headers"
        )
    endif()
    foreach(
        required_child_surface
        IN ITEMS
            "PROC_PIDTASKINFO"
            "PROC_PIDLISTFDS"
            "/proc/self/task"
            "/proc/self/fd"
            "pipe(capabilityPipe)"
            "stable_descriptor_inventory(capabilityPipe[0], capabilityPipe[1])"
            "fork()"
            "dup2(capabilityPipe[0], STDIN_FILENO)"
            "descriptorData[index]"
            "fcntl(forbiddenDescriptor, F_GETFD)"
            "clear_parent_environment()"
            "run_designated_live_mutation_canary(STDIN_FILENO, STDOUT_FILENO, true)"
            "waitpid(child, &status, WNOHANG)"
            "kill(aChild, SIGKILL)"
    )
        string(
            FIND
            "${designated_live_test_child_contents}"
            "${required_child_surface}"
            required_child_surface_index
        )
        if(required_child_surface_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live fork child lacks required fixed surface: "
                "${required_child_surface}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "(^|[^A-Za-z0-9_])fork[ \\t\\r\\n]*[(]"
        designated_live_test_fork_calls
        "${designated_live_test_child_contents}"
    )
    list(LENGTH designated_live_test_fork_calls designated_live_test_fork_call_count)
    if(NOT designated_live_test_fork_call_count EQUAL 1)
        message(FATAL_ERROR "Designated live child must contain exactly one fork")
    endif()
    if(
        designated_live_test_child_contents
            MATCHES
                "(QProcess|QNetwork|QTcp|QUdp|QUrl|std::iostream|std::fstream|std::filesystem|std::system|(^|[^A-Za-z0-9_])(system|popen|exec[a-zA-Z0-9_]*|posix_spawn[a-zA-Z0-9_]*|spawn[a-zA-Z0-9_]*|socket|curl|fopen|open|unlink|remove|rename|link|mkdir|malloc|calloc|realloc|free)[ \\t\\r\\n]*[(]|https?://)"
    )
        message(
            FATAL_ERROR
            "Designated live fork child contains forbidden alternate process, network, allocation, or pathname authority"
        )
    endif()

    set(designated_live_test_cmake "${BPR_ROOT}/tests/CMakeLists.txt")
    if(NOT EXISTS "${designated_live_test_cmake}")
        message(FATAL_ERROR "Designated live runner target composition is missing")
    endif()
    file(READ "${designated_live_test_cmake}" designated_live_test_cmake_contents)
    string(REPLACE "\n" " " designated_live_test_cmake_normalized "${designated_live_test_cmake_contents}")
    string(REPLACE "\r" " " designated_live_test_cmake_normalized "${designated_live_test_cmake_normalized}")
    string(REPLACE "\t" " " designated_live_test_cmake_normalized "${designated_live_test_cmake_normalized}")
    string(REGEX REPLACE " +" " " designated_live_test_cmake_normalized "${designated_live_test_cmake_normalized}")
    set(
        designated_live_expected_runner_sources
        [=[bpr_add_executable( tool_better_pull_review_designated_live_mutation_runner "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp" "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_child.cpp" "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate.cpp" "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_runner.cpp" )]=]
    )
    set(
        designated_live_expected_runner_features
        [=[target_compile_features( tool_better_pull_review_designated_live_mutation_runner PRIVATE cxx_std_23 )]=]
    )
    set(
        designated_live_expected_runner_links
        [=[target_link_libraries( tool_better_pull_review_designated_live_mutation_runner PRIVATE glaze::glaze Qt6::Core Qt6::Network )]=]
    )
    set(
        designated_live_expected_runner_definitions
        [=[target_compile_definitions( tool_better_pull_review_designated_live_mutation_runner PRIVATE BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1 BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256="${BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256}" )]=]
    )
    foreach(
        exact_runner_fragment
        IN ITEMS
            "${designated_live_expected_runner_sources}"
            "${designated_live_expected_runner_features}"
            "${designated_live_expected_runner_links}"
            "${designated_live_expected_runner_definitions}"
            "bpr_enable_hardening(tool_better_pull_review_designated_live_mutation_runner)"
            "bpr_enable_warnings(tool_better_pull_review_designated_live_mutation_runner)"
            [=[set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${bpr_designated_live_canary_surface_path}" )]=]
            [=[string( SHA256 BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256 "${bpr_designated_live_canary_surface_material}" )]=]
    )
        string(
            FIND
            "${designated_live_test_cmake_normalized}"
            "${exact_runner_fragment}"
            exact_runner_fragment_index
        )
        if(exact_runner_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live runner target composition is not exact: ${exact_runner_fragment}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "tool_better_pull_review_designated_live_mutation_runner"
        designated_live_runner_target_mentions
        "${designated_live_test_cmake_contents}"
    )
    list(LENGTH designated_live_runner_target_mentions designated_live_runner_target_mention_count)
    if(NOT designated_live_runner_target_mention_count EQUAL 6)
        message(
            FATAL_ERROR
            "Designated live regression runner target must have exactly its six fixed composition uses"
        )
    endif()
endif()

foreach(qt_github_projection_file IN LISTS qt_github_projection_files)
    file(READ "${qt_github_projection_file}" contents)
    strip_markdown_resource_denial("${qt_github_projection_file}")
    string(
        REGEX MATCHALL
        "#[ \t]*include[ \t]*[<\"]bpr/application/github_[A-Za-z0-9_]+\\.hpp[>\"]"
        qt_application_github_includes
        "${contents}"
    )
    foreach(qt_application_github_include IN LISTS qt_application_github_includes)
        if(
            NOT qt_application_github_include
                MATCHES
                    "bpr/application/github_projection\\.hpp[>\"]$"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${qt_github_projection_file}"
            )
            message(
                FATAL_ERROR
                "Qt adapter GitHub projection boundary violation in "
                "${relative_file}: only "
                "bpr/application/github_projection.hpp may cross from the "
                "application GitHub surface"
            )
        endif()
    endforeach()

    foreach(pattern IN LISTS qt_github_projection_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${qt_github_projection_file}"
            )
            message(
                FATAL_ERROR
                "Qt adapter GitHub projection boundary violation in "
                "${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

file(READ "${BPR_ROOT}/src/application/CMakeLists.txt" application_cmake_contents)
if(
    application_cmake_contents
        MATCHES
            "BetterPullReview::(AdapterGitHub|AdapterQt)|Qt6::|Q[A-Za-z]+::"
)
    message(
        FATAL_ERROR
        "Application target must depend inward on Core only"
    )
endif()

set(app_cmake_contents "")
if(EXISTS "${BPR_ROOT}/src/app/CMakeLists.txt")
    file(READ "${BPR_ROOT}/src/app/CMakeLists.txt" app_cmake_contents)
endif()

file(READ "${BPR_ROOT}/src/adapters/qt/CMakeLists.txt" qt_cmake_contents)
if(qt_cmake_contents MATCHES "BetterPullReview::AdapterGitHub")
    message(
        FATAL_ERROR
        "Qt presentation adapter must not link the GitHub provider adapter"
    )
endif()

file(
    GLOB_RECURSE source_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/*.c"
    "${BPR_ROOT}/src/*.cc"
    "${BPR_ROOT}/src/*.cpp"
    "${BPR_ROOT}/src/*.cxx"
    "${BPR_ROOT}/src/*.h"
    "${BPR_ROOT}/src/*.hh"
    "${BPR_ROOT}/src/*.hpp"
    "${BPR_ROOT}/src/*.hxx"
    "${BPR_ROOT}/src/*.m"
    "${BPR_ROOT}/src/*.mm"
)

set(core_cmake_contents "")
if(EXISTS "${core_cmake}")
    file(READ "${core_cmake}" core_cmake_contents)
endif()

# [[BPR-0825]]. The header-only platform target: definitions several adapters
# share and none owns, starting with the Windows owner-private object.
set(
    platform_windows_private_object_relative
    "src/platform/include/bpr/platform/windows_private_object.hpp"
)
# [[BPR-1115]]. The Windows launch restriction the QProcess launchers share: a
# child inherits its three standard handles and nothing else, and a launch whose
# list cannot be built is refused by process creation.
set(
    platform_windows_standard_handles_relative
    "src/platform/include/bpr/platform/windows_standard_handles.hpp"
)
set(platform_cmake_contents "")
if(EXISTS "${BPR_ROOT}/src/platform/CMakeLists.txt")
    file(READ "${BPR_ROOT}/src/platform/CMakeLists.txt" platform_cmake_contents)
endif()

function(verify_effect_owner effect_pattern effect_name allowed_relative_file)
    set(allowed_relative_files "${allowed_relative_file}" ${ARGN})
    set(allowed_files "")
    foreach(current_allowed_relative IN LISTS allowed_relative_files)
        get_filename_component(
            current_allowed
            "${BPR_ROOT}/${current_allowed_relative}"
            ABSOLUTE
        )
        list(APPEND allowed_files "${current_allowed}")
    endforeach()

    foreach(source_file IN LISTS source_files)
        file(READ "${source_file}" contents)
        if(NOT contents MATCHES "${effect_pattern}")
            continue()
        endif()

        get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
        if(NOT normalized_source_file IN_LIST allowed_files)
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "${effect_name} ownership violation in ${relative_file}; "
                "only ${allowed_relative_files} may use it"
            )
        endif()
    endforeach()

    foreach(current_allowed IN LISTS allowed_files)
        if(NOT EXISTS "${current_allowed}")
            continue()
        endif()
        get_filename_component(allowed_name "${current_allowed}" NAME)
        file(
            RELATIVE_PATH
            current_allowed_relative
            "${BPR_ROOT}"
            "${current_allowed}"
        )
        if(
            current_allowed_relative
                STREQUAL github_designated_live_observer_relative
        )
            set(effect_owner_cmake "${github_cmake_contents}")
            set(
                effect_owner_target
                "better_pull_review_designated_live_public_observer"
            )
        elseif(
            current_allowed_relative
                STREQUAL app_designated_live_supervisor_relative
            OR current_allowed_relative
                STREQUAL app_designated_live_supervisor_launch_relative
        )
            set(effect_owner_cmake "${app_cmake_contents}")
            set(
                effect_owner_target
                "tool_better_pull_review_designated_live_supervisor"
            )
        elseif(
            current_allowed_relative
                STREQUAL app_designated_live_provenance_relative
        )
            # Compiled into the application and the supervisor both: it is one
            # half of a handshake each performs, so a single owner would be the
            # wrong answer. src/app/CMakeLists.txt declares it for both.
            set(effect_owner_cmake "${app_cmake_contents}")
            set(effect_owner_target "app_better_pull_review")
        elseif(current_allowed_relative MATCHES "^src/adapters/process_support/")
            set(effect_owner_cmake "")
            if(EXISTS "${BPR_ROOT}/src/adapters/process_support/CMakeLists.txt")
                file(READ "${BPR_ROOT}/src/adapters/process_support/CMakeLists.txt" effect_owner_cmake)
            endif()
            set(effect_owner_target "better_pull_review_process_support")
        elseif(current_allowed_relative MATCHES "^src/adapters/git/")
            set(effect_owner_cmake "${git_cmake_contents}")
            set(effect_owner_target "better_pull_review_adapter_git")
        elseif(current_allowed_relative MATCHES "^src/adapters/agent/")
            set(effect_owner_cmake "${agent_cmake_contents}")
            set(effect_owner_target "better_pull_review_adapter_agent")
        elseif(current_allowed_relative STREQUAL "src/adapters/update/src/macos_release_update.cpp"
            OR current_allowed_relative STREQUAL "src/adapters/update/src/macos_update_helper.cpp")
            file(READ "${BPR_ROOT}/src/adapters/update/CMakeLists.txt" effect_owner_cmake)
            set(effect_owner_target "better_pull_review_adapter_update")
        elseif(current_allowed_relative STREQUAL "src/adapters/mcp/src/mcp_terminal_diagnostic.hpp")
            file(READ "${BPR_ROOT}/src/adapters/mcp/CMakeLists.txt" effect_owner_cmake)
            set(effect_owner_target "tool_better_pull_review_mcp_context_helper")
        elseif(current_allowed_relative STREQUAL "src/app/src/tour_editor.cpp")
            set(effect_owner_cmake "${app_cmake_contents}")
            set(effect_owner_target "better_pull_review_tour_runtime")
        elseif(current_allowed_relative STREQUAL "src/app/src/terminal_diagnostics.cpp")
            set(effect_owner_cmake "${app_cmake_contents}")
            set(effect_owner_target "better_pull_review_composition")
        elseif(current_allowed_relative MATCHES "^src/core/")
            set(effect_owner_cmake "${core_cmake_contents}")
            set(effect_owner_target "better_pull_review_core")
        elseif(current_allowed_relative MATCHES "^src/platform/")
            set(effect_owner_cmake "${platform_cmake_contents}")
            set(effect_owner_target "better_pull_review_platform")
        else()
            set(effect_owner_cmake "${github_cmake_contents}")
            set(effect_owner_target "better_pull_review_adapter_github")
        endif()
        if(NOT effect_owner_cmake MATCHES "${allowed_name}")
            message(
                FATAL_ERROR
                "Designated effect source ${allowed_name} is not owned by "
                "${effect_owner_target}"
            )
        endif()
    endforeach()
endfunction()

verify_effect_owner(
    "QProcess"
    "QProcess"
    "src/app/src/tour_editor.cpp"
    "${github_process_runner_relative}"
    "${git_process_runner_relative}"
    "${process_support_source_relative}"
    "${process_support_header_relative}"
    "${agent_process_runner_relative}"
    "src/adapters/update/src/macos_release_update.cpp"
)
# The manual tour editor is an explicit trusted-argv terminal capability.
# It never parses a shell command, selects repository configuration, or detaches.
if(EXISTS "${BPR_ROOT}/src/app/src/tour_editor.cpp")
    file(READ "${BPR_ROOT}/src/app/src/tour_editor.cpp" tour_editor_contents)
    foreach(required_editor_boundary
        "BPR_TOUR_EDITOR_ARGV" "QDir::isAbsolutePath" "QTemporaryDir"
        "QIODevice::NewOnly" "read_projection" "CloseFileDescriptors"
        "windows_standard_handles::launch_restriction" "inheritance.apply"
        "O_NOFOLLOW" "FILE_FLAG_OPEN_REPARSE_POINT" "process.waitForFinished")
        string(FIND "${tour_editor_contents}" "${required_editor_boundary}" editor_boundary_found)
        if(editor_boundary_found LESS 0)
            message(FATAL_ERROR "Tour editor lost its ${required_editor_boundary} boundary")
        endif()
    endforeach()
    if(tour_editor_contents MATCHES "(startCommand|splitCommand|setNativeArguments|startDetached|QProcess::execute|QSettings)")
        message(FATAL_ERROR "Tour editor must retain explicit trusted argv and attended process ownership")
    endif()
endif()

verify_effect_owner(
    "QProcessEnvironment"
    "QProcessEnvironment"
    "${github_process_runner_relative}"
    "${git_process_runner_relative}"
    "${agent_process_runner_relative}"
    "src/adapters/update/src/macos_release_update.cpp"
)
verify_effect_owner(
    "CreateProcess(AsUser|WithLogon|WithToken)?[AW]?[ \t\r\n]*\\("
    "native Windows process creation API"
    "${agent_windows_process_relative}"
    "${agent_windows_supervisor_relative}"
)
verify_effect_owner(
    "(CreateJobObjectW|SetInformationJobObject|AssignProcessToJobObject|ResumeThread|TerminateJobObject|QueryInformationJobObject|InitializeProcThreadAttributeList|UpdateProcThreadAttribute|DeleteProcThreadAttributeList|PROC_THREAD_ATTRIBUTE_HANDLE_LIST)"
    "native Windows agent containment API"
    "${git_process_runner_relative}"
    "${agent_windows_process_relative}"
    "${agent_windows_supervisor_relative}"
    # [[BPR-1115]]. The handle list a QProcess launcher applies is built here,
    # once, rather than in each launcher.
    "${platform_windows_standard_handles_relative}"
)
# BPR-0972: cancelling one owned blocking write is not process containment.
# BPR-0971's terminal owners need only this API; Job Object and inherited-handle
# APIs above remain confined to their existing process/platform owners.
verify_effect_owner(
    "CancelSynchronousIo"
    "native Windows synchronous I/O cancellation API"
    "${git_process_runner_relative}"
    "${agent_windows_process_relative}"
    "${agent_windows_supervisor_relative}"
    "src/adapters/mcp/src/mcp_terminal_diagnostic.hpp"
    "src/app/src/terminal_diagnostics.cpp"
)
verify_effect_owner(
    "QNetworkAccessManager"
    "QNetworkAccessManager"
    "${github_native_https_relative}"
)
verify_effect_owner(
    "(QNetwork[A-Za-z0-9_]*|QSsl[A-Za-z0-9_]*)"
    "Qt network/TLS type"
    "${github_native_https_relative}"
)
verify_effect_owner(
    "GH_TELEMETRY=0"
    "GitHub CLI telemetry hardening"
    "${github_cli_transport_relative}"
)
verify_effect_owner(
    "(Security/(SecItem|SecKeychain)\\.h|Sec(Item|Keychain)[A-Za-z0-9_]*|kSec(Class|Attr|Use|Match|Return)[A-Za-z0-9_]*)"
    "macOS Keychain API"
    "${macos_keychain_relative}"
)
verify_effect_owner(
    "(Sec(Static)?Code[A-Za-z0-9_]*|SecRequirement[A-Za-z0-9_]*|kSec(Code|CS|Guest)[A-Za-z0-9_]*)"
    "macOS code-signing API"
    "${github_designated_live_observer_relative}"
    "${app_designated_live_supervisor_relative}"
    # BPR-0351. The authenticated launch channel identifies its peer from the
    # kernel audit trailer, which is a code-signing question by construction.
    "${app_designated_live_provenance_relative}"
    # BPR-0947. Installed and staged application identities are compared only
    # by this native updater effect owner, never by core or presentation.
    "src/adapters/update/src/macos_release_update.cpp"
)
verify_effect_owner(
    "(/usr/bin/curl|com\\.apple\\.curl)"
    "designated live public curl/network"
    "${github_designated_live_observer_relative}"
)
verify_effect_owner(
    "(^|[^A-Za-z0-9_])posix_spawn[ \\t\\r\\n]*\\("
    "posix_spawn"
    "${github_designated_live_observer_relative}"
    "${app_designated_live_supervisor_launch_relative}"
    "src/adapters/update/src/macos_update_helper.cpp"
)
verify_effect_owner(
    "(wincred\\.h|Cred(Read|Write|Delete|Free|Enumerate|Rename|Protect|Unprotect|Pack|UnPack|IsProtected|FindBest|GetSessionTypes|Marshal|Unmarshal)[A-Za-z0-9_]*|P?CREDENTIAL(A|W)?([^A-Z0-9_]|$)|CRED_[A-Z0-9_]+)"
    "Windows Credential Manager API"
    "${windows_credential_manager_relative}"
)

# [[BPR-0825]]. One definition of the Windows owner-private descriptor. The
# SQLite and logging storage roots and the agent and Git workspace objects are
# all created with the platform target's descriptor, and the Git adapter admits
# the data root the SQLite adapter created only as that descriptor. A second
# builder anywhere else could drift from it and silently turn Windows workspace
# ownership into repair_required, so only the platform header builds one.
verify_effect_owner(
    "(InitializeAcl|AddAccessAllowed(Object)?Ace(Ex)?|SetSecurityDescriptor(Owner|Dacl|Control))[ \t\r\n]*\\("
    "Windows owner-private descriptor construction"
    "${platform_windows_private_object_relative}"
)
# The platform target is neutral: it depends on no first-party target and
# includes nothing from Qt, a provider or another layer, so an adapter can take
# it without an adapter-to-adapter edge.
if(NOT platform_cmake_contents STREQUAL "")
    if(
        NOT platform_cmake_contents
            MATCHES "add_library\\([ \t\r\n]*better_pull_review_platform[ \t\r\n]+INTERFACE[ \t\r\n]*\\)"
    )
        message(
            FATAL_ERROR
            "Platform target must be the header-only better_pull_review_platform"
        )
    endif()
    string(
        REPLACE
            "add_library(BetterPullReview::Platform ALIAS better_pull_review_platform)"
            ""
            platform_cmake_dependencies
            "${platform_cmake_contents}"
    )
    if(
        platform_cmake_dependencies
            MATCHES "(BetterPullReview::|better_pull_review_(core|application|adapter)|Qt6::|quill|glaze)"
    )
        message(
            FATAL_ERROR
            "Platform target must not depend on a first-party layer, Qt or a provider"
        )
    endif()
endif()
file(
    GLOB_RECURSE platform_source_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/src/platform/*.h"
    "${BPR_ROOT}/src/platform/*.hpp"
    "${BPR_ROOT}/src/platform/*.cpp"
)
foreach(platform_source IN LISTS platform_source_files)
    file(READ "${platform_source}" platform_source_contents)
    if(
        platform_source_contents
            MATCHES "#[ \t]*include[ \t]*[<\"](Q[A-Za-z]|qt|bpr/(core|application|adapters|app)/|quill/|sqlite|glaze/)"
    )
        file(RELATIVE_PATH platform_source_relative "${BPR_ROOT}" "${platform_source}")
        message(
            FATAL_ERROR
            "Platform target must stay free of Qt, providers and first-party "
            "layers: ${platform_source_relative}"
        )
    endif()
endforeach()

get_filename_component(
    github_designated_live_observer_file
    "${BPR_ROOT}/${github_designated_live_observer_relative}"
    ABSOLUTE
)
get_filename_component(
    github_designated_live_observer_interface_file
    "${BPR_ROOT}/${github_designated_live_observer_interface_relative}"
    ABSOLUTE
)
if(EXISTS "${github_designated_live_observer_file}")
    if(NOT EXISTS "${github_designated_live_observer_interface_file}")
        message(FATAL_ERROR "Designated live public observer interface is missing")
    endif()
    file(
        READ
        "${github_designated_live_observer_file}"
        github_designated_live_observer_contents
    )
    file(
        READ
        "${github_designated_live_observer_interface_file}"
        github_designated_live_observer_interface_contents
    )
    foreach(
        required_observer_fragment
        IN ITEMS
            [=["/usr/bin/curl"]=]
            [=["https://api.github.com"]=]
            [=["/repos/MikeMarcin/BPR-Test/pulls/2"]=]
            [=[CFSTR("anchor apple and identifier \"com.apple.curl\"")]=]
            [=[kSecCSCheckAllArchitectures]=]
            [=[POSIX_SPAWN_START_SUSPENDED]=]
            [=[std::array<char*, 1>{ nullptr }]=]
            [=["--disable"]=]
            [=["--request"]=]
            [=["GET"]=]
            [=["--proto"]=]
            [=["=https"]=]
            [=["--tlsv1.2"]=]
            [=["--tls-max"]=]
            [=["1.3"]=]
            [=["--http1.1"]=]
            [=["--noproxy"]=]
            [=["--proxy"]=]
            [=["--max-redirs"]=]
            [=["--connect-timeout"]=]
            [=["--max-time"]=]
            [=["--max-filesize"]=]
            [=["Accept: application/vnd.github+json"]=]
            [=["User-Agent: BetterPullReview/0.1"]=]
            [=["X-GitHub-Api-Version: 2022-11-28"]=]
            [=["Accept-Encoding: identity"]=]
            [=["Accept-Language: *"]=]
            [=[designated_live_public_observation_failure::cancelled]=]
            [=[designated_live_public_observation_failure::timeout]=]
            [=[response_header_limit]=]
            [=[response_body_limit]=]
    )
        string(
            FIND
            "${github_designated_live_observer_contents}"
            "${required_observer_fragment}"
            required_observer_fragment_index
        )
        if(required_observer_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live public observer policy is incomplete: "
                "${required_observer_fragment}"
            )
        endif()
    endforeach()
    foreach(
        forbidden_observer_pattern
        IN ITEMS
            "(^|[^A-Za-z0-9_])(getenv|setenv|putenv)[ \\t\\r\\n]*\\("
            "(^|[^A-Za-z0-9_])(system|popen|fork|exec[a-zA-Z0-9_]*|posix_spawnp)[ \\t\\r\\n]*\\("
            "(QProcess|QNetwork|QTcp|QUdp|NSURLSession|CFNetwork|curl_easy_|libcurl)"
            "\"--(location|netrc|cookie|user|oauth2-bearer)\""
    )
        if(
            github_designated_live_observer_contents
                MATCHES "${forbidden_observer_pattern}"
        )
            message(
                FATAL_ERROR
                "Designated live public observer acquired forbidden ambient, "
                "redirect, credential, shell, or second-network authority: "
                "${forbidden_observer_pattern}"
            )
        endif()
    endforeach()
    foreach(
        required_observer_interface_fragment
        IN ITEMS
            [=[observe_designated_live_public_target(]=]
            [=[std::stop_token aStop = { })]=]
            [=[#if defined(BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING)]=]
    )
        string(
            FIND
            "${github_designated_live_observer_interface_contents}"
            "${required_observer_interface_fragment}"
            required_observer_interface_fragment_index
        )
        if(required_observer_interface_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live public observer interface is not a fixed "
                "production operation: ${required_observer_interface_fragment}"
            )
        endif()
    endforeach()
    foreach(
        required_observer_cmake_fragment
        IN ITEMS
            [=[add_library( better_pull_review_designated_live_public_observer STATIC src/designated_live_public_observer.cpp )]=]
            [=[if(APPLE AND (BUILD_TESTING OR BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE))]=]
            [=[target_compile_definitions( better_pull_review_designated_live_public_observer PRIVATE BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING=1 )]=]
    )
        string(
            FIND
            "${github_cmake_compact}"
            "${required_observer_cmake_fragment}"
            required_observer_cmake_fragment_index
        )
        if(required_observer_cmake_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live public observer build ownership or test-seam "
                "isolation is incomplete: ${required_observer_cmake_fragment}"
            )
        endif()
    endforeach()

endif()

get_filename_component(
    app_designated_live_supervisor_file
    "${BPR_ROOT}/${app_designated_live_supervisor_relative}"
    ABSOLUTE
)
get_filename_component(
    app_designated_live_supervisor_identity_file
    "${BPR_ROOT}/${app_designated_live_supervisor_identity_relative}"
    ABSOLUTE
)
get_filename_component(
    app_designated_live_supervisor_launch_file
    "${BPR_ROOT}/${app_designated_live_supervisor_launch_relative}"
    ABSOLUTE
)
get_filename_component(
    designated_live_macos_capsule_script_file
    "${BPR_ROOT}/${designated_live_macos_capsule_script_relative}"
    ABSOLUTE
)
get_filename_component(
    designated_live_stage_zero_preloader_file
    "${BPR_ROOT}/${designated_live_stage_zero_preloader_relative}"
    ABSOLUTE
)
get_filename_component(
    designated_live_test_isolation_file
    "${BPR_ROOT}/${designated_live_test_isolation_relative}"
    ABSOLUTE
)
if(EXISTS "${app_designated_live_supervisor_file}")
    file(
        READ
        "${app_designated_live_supervisor_file}"
        app_designated_live_supervisor_contents
    )
    foreach(
        required_supervisor_fragment
        IN ITEMS
            [=[session::open(]=]
            [=[run_static_verifier(*retainedSession)]=]
            [=[observe_designated_live_public_target()]=]
            [=[attended->Approval, attended->Profile, *retainedSession]=]
            [=[designated_live_supervisor_launch::verify_static(]=]
            [=[verifier_operations::production()]=]
            [=[designated_live_supervisor_launch::launch(]=]
            [=[restored = channel->restore()]=]
            [=[if (!restored || !channel.has_value()]=]
            [=[POSIX_SPAWN_START_SUSPENDED]=]
            [=[guest_signing_digest(aProcess)]=]
            [=["--attended-designated-live-approval"]=]
    )
        string(
            FIND
            "${app_designated_live_supervisor_contents}"
            "${required_supervisor_fragment}"
            required_supervisor_fragment_index
        )
        if(required_supervisor_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live supervisor prelaunch policy is incomplete: "
                "${required_supervisor_fragment}"
            )
        endif()
    endforeach()
    if(app_designated_live_supervisor_contents MATCHES "SIGCONT")
        message(
            FATAL_ERROR
            "Designated live supervisor must delegate application resume to "
            "the shared launch controller"
        )
    endif()
    foreach(
        required_supervisor_build_fragment
        IN ITEMS
            [=[file( SHA256 "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py" BPR_DESIGNATED_LIVE_VERIFIER_SHA256 )]=]
            [=[target_compile_definitions( tool_better_pull_review_designated_live_supervisor PRIVATE]=]
            [=["BPR_DESIGNATED_LIVE_VERIFIER_SHA256]=]
            [=[src/designated_live_macos_supervisor_launch.cpp]=]
            [=[bpr_designated_live_stage_zero_command(]=]
            [=[bpr_designated_live_probe_command]=]
            [=[bpr_designated_live_capsule_image]=]
            [=[bpr_designated_live_source_manifest]=]
    )
        string(
            REGEX REPLACE
            "[ \t\r\n]+"
            " "
            app_cmake_compact
            "${app_cmake_contents}"
        )
        string(
            FIND
            "${app_cmake_compact}"
            "${required_supervisor_build_fragment}"
            required_supervisor_build_fragment_index
        )
        if(required_supervisor_build_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live supervisor trusted verifier or real probe "
                "build binding is incomplete: ${required_supervisor_build_fragment}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "observe_designated_live_public_target[ \t\r\n]*[(][ \t\r\n]*[)]"
        designated_live_supervisor_observation_calls
        "${app_designated_live_supervisor_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_observation_calls
        designated_live_supervisor_observation_call_count
    )
    if(NOT designated_live_supervisor_observation_call_count EQUAL 1)
        message(
            FATAL_ERROR
            "Designated live supervisor must perform exactly one fixed public observation"
        )
    endif()
    string(
        REGEX MATCHALL
        "designated_live_supervisor_launch::launch[ \t\r\n]*[(]"
        designated_live_supervisor_launch_calls
        "${app_designated_live_supervisor_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_launch_calls
        designated_live_supervisor_launch_call_count
    )
    if(NOT designated_live_supervisor_launch_call_count EQUAL 2)
        message(
            FATAL_ERROR
            "Designated live supervisor prelaunch policy is incomplete: "
            "probe and attended launches must use the shared controller"
        )
    endif()
    string(
        FIND
        "${app_designated_live_supervisor_contents}"
        "session::open("
        designated_live_supervisor_retention_index
    )
    string(
        FIND
        "${app_designated_live_supervisor_contents}"
        "run_static_verifier(*retainedSession)"
        designated_live_supervisor_verifier_index
    )
    string(
        FIND
        "${app_designated_live_supervisor_contents}"
        "observe_designated_live_public_target()"
        designated_live_supervisor_observation_index
    )
    string(
        FIND
        "${app_designated_live_supervisor_contents}"
        "attended->Approval, attended->Profile, *retainedSession"
        designated_live_supervisor_launch_index
    )
    if(
        designated_live_supervisor_retention_index GREATER_EQUAL
            designated_live_supervisor_verifier_index
        OR designated_live_supervisor_verifier_index GREATER_EQUAL
            designated_live_supervisor_observation_index
        OR designated_live_supervisor_observation_index GREATER_EQUAL
            designated_live_supervisor_launch_index
    )
        message(
            FATAL_ERROR
            "Designated live supervisor must retain identity before verification, "
            "then observe the fixed public target before application launch"
        )
    endif()
endif()

if(EXISTS "${app_designated_live_supervisor_launch_file}")
    file(
        READ
        "${app_designated_live_supervisor_launch_file}"
        app_designated_live_supervisor_launch_contents
    )
    foreach(
        required_supervisor_launch_fragment
        IN ITEMS
            [=[aApplication != fixedApplication]=]
            [=[aApplication.is_absolute()]=]
            [=[fixedApplication.c_str()]=]
            [=[aOperations.Barrier(phase::BeforeSpawn)]=]
            [=[aOperations.Spawn(]=]
            [=[aOperations.PostSpawnCheck(]=]
            [=[aOperations.Barrier(phase::BeforeResume)]=]
            [=[aOperations.Resume(]=]
            [=[aOperations.Refuse(]=]
            [=[aSession.configure_verifier_spawn(actions)]=]
            [=[python.c_str()]=]
            [=[std::string{ "." }]=]
            [=[std::string{ "--inventory-fd" }]=]
            [=[aSession.transmit_verifier_and_inventory(*transport)]=]
            [=[result.Revalidated = aSession.revalidate()]=]
            [=[terminate_and_wait]=]
    )
        string(
            FIND
            "${app_designated_live_supervisor_launch_contents}"
            "${required_supervisor_launch_fragment}"
            required_supervisor_launch_fragment_index
        )
        if(required_supervisor_launch_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live supervisor launch controller is incomplete: "
                "${required_supervisor_launch_fragment}"
            )
        endif()
    endforeach()
    foreach(
        forbidden_supervisor_verifier_path_fragment
        IN ITEMS
            [=[aSession.root_path()]=]
            [=[std::filesystem::absolute(]=]
            [=[std::filesystem::canonical(]=]
            [=[std::filesystem::weakly_canonical(]=]
            [=["--inventory"]=]
    )
        string(
            FIND
            "${app_designated_live_supervisor_launch_contents}"
            "${forbidden_supervisor_verifier_path_fragment}"
            forbidden_supervisor_verifier_path_fragment_index
        )
        if(NOT forbidden_supervisor_verifier_path_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live supervisor verifier can regain mutable pathname "
                "authority: ${forbidden_supervisor_verifier_path_fragment}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "aSession[.]revalidate[(][)]"
        designated_live_supervisor_controller_revalidations
        "${app_designated_live_supervisor_launch_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_controller_revalidations
        designated_live_supervisor_controller_revalidation_count
    )
    if(NOT designated_live_supervisor_controller_revalidation_count EQUAL 4)
        message(
            FATAL_ERROR
            "Designated live supervisor launch controller is incomplete: "
            "verifier and application must each revalidate twice"
        )
    endif()
    string(
        REGEX MATCHALL
        "aOperations[.]Spawn[(]"
        designated_live_supervisor_controller_spawns
        "${app_designated_live_supervisor_launch_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_controller_spawns
        designated_live_supervisor_controller_spawn_count
    )
    string(
        REGEX MATCHALL
        "aOperations[.]Refuse[(]"
        designated_live_supervisor_controller_refusals
        "${app_designated_live_supervisor_launch_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_controller_refusals
        designated_live_supervisor_controller_refusal_count
    )
    if(
        NOT designated_live_supervisor_controller_spawn_count EQUAL 2
        OR NOT designated_live_supervisor_controller_refusal_count EQUAL 2
    )
        message(
            FATAL_ERROR
            "Designated live supervisor launch controller is incomplete: "
            "verifier/application spawn and application refusal topology changed"
        )
    endif()
endif()

if(EXISTS "${app_designated_live_supervisor_identity_file}")
    file(
        READ
        "${app_designated_live_supervisor_identity_file}"
        app_designated_live_supervisor_identity_contents
    )
    foreach(
        required_supervisor_identity_fragment
        IN ITEMS
            [=[designated_live_capsule_identity::capture(]=]
            [=[open_verifier(result.RootDescriptor)]=]
            [=[sha256_hex(*verifierBytes)]=]
            [=[result.VerifierSha256 != aTrustedVerifierSha256]=]
            [=[sha256_hex(result.InventoryBytes)]=]
            [=[RootParentDescriptor, RootName, RootIdentity]=]
            [=[InventoryParentDescriptor,]=]
            [=[InventoryName,]=]
            [=[InventoryIdentity]=]
            [=[add_retained_directory_action(aActions, RootDescriptor)]=]
            [=[posix_spawn_file_actions_addfchdir(&aActions, aDescriptor)]=]
            [=[posix_spawn_file_actions_addfchdir_np(&aActions, aDescriptor)]=]
            [=[::socketpair(]=]
            [=[SO_NOSIGPIPE]=]
            [=[posix_spawn_file_actions_adddup2(]=]
            [=[verifier_child_descriptor()]=]
            [=[inventory_child_descriptor()]=]
            [=[transmit_stream(]=]
    )
        string(
            FIND
            "${app_designated_live_supervisor_identity_contents}"
            "${required_supervisor_identity_fragment}"
            required_supervisor_identity_fragment_index
        )
        if(required_supervisor_identity_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live supervisor retained identity is incomplete: "
                "${required_supervisor_identity_fragment}"
            )
        endif()
    endforeach()
    string(
        REGEX MATCHALL
        "add_retained_directory_action[(]aActions, RootDescriptor[)]"
        designated_live_supervisor_retained_root_actions
        "${app_designated_live_supervisor_identity_contents}"
    )
    list(
        LENGTH
        designated_live_supervisor_retained_root_actions
        designated_live_supervisor_retained_root_action_count
    )
    if(NOT designated_live_supervisor_retained_root_action_count EQUAL 2)
        message(
            FATAL_ERROR
            "Designated live supervisor retained identity is incomplete: "
            "the verifier and application must both inherit the retained root"
        )
    endif()
endif()

if(EXISTS "${designated_live_macos_capsule_script_file}")
    file(
        READ
        "${designated_live_macos_capsule_script_file}"
        designated_live_macos_capsule_script_contents
    )
    foreach(
        required_retained_verifier_fragment
        IN ITEMS
            [=[retained_root = arguments.inventory_fd is not None]=]
            [=[arguments.capsule != pathlib.Path(".")]=]
            [=[retained_relative_path(path.parent / target)]=]
            [=[inventory_entries(root, retained_root=retained_root)]=]
            [=[verify_dependencies(root, arguments.otool, retained_root=retained_root)]=]
    )
        string(
            FIND
            "${designated_live_macos_capsule_script_contents}"
            "${required_retained_verifier_fragment}"
            required_retained_verifier_fragment_index
        )
        if(required_retained_verifier_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live verifier can regain mutable pathname authority: "
                "${required_retained_verifier_fragment}"
            )
        endif()
    endforeach()

    foreach(
        required_stage_zero_fragment
        IN ITEMS
            [=[retained_image = retain_image(arguments.image)]=]
            [=[with signals.defer_interrupts():
                attach_read_only(retained_image.path, root, arguments.hdiutil)
                attached = True]=]
            [=[attach_read_only(retained_image.path, root, arguments.hdiutil)]=]
            [=[retained_image = rebaseline_attached_image(retained_image)]=]
            [=["-readonly"]=]
            [=["-nobrowse"]=]
            [=["-noautoopen"]=]
            [=[mounted = capture_runtime_identity(root)]=]
            [=[mount_descriptor = os.open(]=]
            [=[capsule_descriptor = os.open(]=]
            [=[filesystem_identity(os.fstat(expected.mount_descriptor))]=]
            [=[filesystem_identity(os.fstat(expected.capsule_descriptor))]=]
            [=[if filesystem_identity(os.fstat(image.descriptor)) != image.identity:]=]
            [=[if filesystem_identity(os.fstat(path_descriptor)) != image.identity:]=]
            [=[approval = approval_path(arguments, root)]=]
            [=[inventory = root / IMAGE_INVENTORY_RELATIVE]=]
            [=[helper = root / IMAGE_CAPSULE_RELATIVE / HELPER_RELATIVE]=]
            [=[launched = run_stage_one(helper, inventory, approval, signals, **profile_arguments)]=]
            [=[if launched.returncode != 0:]=]
            [=[STAGE_ONE_TIMEOUT_SECONDS = 1500.0]=]
            [=[STAGE_ONE_TERM_GRACE_SECONDS = 10.0]=]
            [=[STAGE_ONE_KILL_GRACE_SECONDS = 10.0]=]
            [=[STAGE_ZERO_SIGNALS = (signal.SIGTERM, signal.SIGINT, signal.SIGHUP)]=]
            [=[signal.signal(signum, self.handle)]=]
            [=[signal.signal(signum, signal.SIG_IGN)]=]
            [=[subprocess.Popen(arguments, start_new_session=True)]=]
            [=[process.wait(timeout=STAGE_ONE_TIMEOUT_SECONDS)]=]
            [=[signals.defer_interrupts()
        if signals is not None
        else contextlib.nullcontext()]=]
            [=[signal_process_group(process_group, signal.SIGTERM)]=]
            [=[process.wait(timeout=max(0.0, term_deadline - time.monotonic()))]=]
            [=[wait_for_process_group_exit(process_group, term_deadline)]=]
            [=[signal_process_group(process_group, signal.SIGKILL)]=]
            [=[process.wait(timeout=max(0.0, kill_deadline - time.monotonic()))]=]
            [=[wait_for_process_group_exit(process_group, kill_deadline)]=]
            [=[except BaseException:
        if process is not None and process_group_exists(process.pid):]=]
            [=[with scoped_stage_zero_signals() as signals:]=]
            [=[scope.install()]=]
            [=[scope.restore()]=]
            [=[finally:
            signals.begin_cleanup()
            if mounted is not None:]=]
            [=[close_runtime_identity(mounted)]=]
            [=[detach_without_force(root, arguments.hdiutil)]=]
            [=[require_underlying_mount_root(root, underlying)]=]
    )
        string(
            FIND
            "${designated_live_macos_capsule_script_contents}"
            "${required_stage_zero_fragment}"
            required_stage_zero_fragment_index
        )
        if(required_stage_zero_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live image stage zero is incomplete: "
                "${required_stage_zero_fragment}"
            )
        endif()
    endforeach()

    string(
        REGEX REPLACE
        "[ \t\r\n]+"
        " "
        designated_live_macos_capsule_script_compact
        "${designated_live_macos_capsule_script_contents}"
    )
    string(
        REGEX MATCHALL
        "revalidate_image[(]retained_image[)]"
        designated_live_image_revalidations
        "${designated_live_macos_capsule_script_compact}"
    )
    list(LENGTH designated_live_image_revalidations designated_live_image_revalidation_count)
    string(
        REGEX MATCHALL
        "revalidate_runtime_identity[(][ ]*root, mounted, arguments[.]source_sha256,?[ ]*[)]"
        designated_live_runtime_revalidations
        "${designated_live_macos_capsule_script_compact}"
    )
    list(LENGTH designated_live_runtime_revalidations designated_live_runtime_revalidation_count)
    string(
        REGEX MATCHALL
        "detach_without_force[(]root, arguments[.]hdiutil[)]"
        designated_live_detach_calls
        "${designated_live_macos_capsule_script_compact}"
    )
    list(LENGTH designated_live_detach_calls designated_live_detach_call_count)
    string(
        REGEX MATCHALL
        "filesystem_identity[(]os[.]fstat[(]image[.]descriptor[)][)] != image[.]identity"
        designated_live_retained_image_identity_checks
        "${designated_live_macos_capsule_script_compact}"
    )
    list(
        LENGTH
        designated_live_retained_image_identity_checks
        designated_live_retained_image_identity_check_count
    )
    string(
        REGEX MATCHALL
        "filesystem_identity[(]os[.]fstat[(]path_descriptor[)][)] != image[.]identity"
        designated_live_path_image_identity_checks
        "${designated_live_macos_capsule_script_compact}"
    )
    list(
        LENGTH
        designated_live_path_image_identity_checks
        designated_live_path_image_identity_check_count
    )
    if(
        NOT designated_live_image_revalidation_count EQUAL 2
        OR NOT designated_live_runtime_revalidation_count EQUAL 2
        OR NOT designated_live_detach_call_count EQUAL 1
    )
        message(
            FATAL_ERROR
            "Designated live image stage zero must revalidate before and after "
            "the child, then detach exactly once"
        )
    endif()
    if(
        NOT designated_live_retained_image_identity_check_count EQUAL 2
        OR NOT designated_live_path_image_identity_check_count EQUAL 2
    )
        message(
            FATAL_ERROR
            "Designated live image hashes must be bracketed by full retained "
            "descriptor and path identity checks"
        )
    endif()
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "launched = run_stage_one(helper, inventory, approval, signals, **profile_arguments)"
        designated_live_child_index
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "revalidate_image(retained_image)"
        designated_live_post_image_index
        REVERSE
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "revalidate_runtime_identity("
        designated_live_post_runtime_index
        REVERSE
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "if launched.returncode != 0:"
        designated_live_child_status_index
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "close_runtime_identity(mounted)"
        designated_live_close_runtime_index
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "detach_without_force(root, arguments.hdiutil)"
        designated_live_detach_index
    )
    if(
        designated_live_child_index GREATER_EQUAL designated_live_post_image_index
        OR designated_live_post_image_index GREATER_EQUAL designated_live_post_runtime_index
        OR designated_live_post_runtime_index GREATER_EQUAL designated_live_child_status_index
        OR designated_live_child_status_index GREATER_EQUAL designated_live_close_runtime_index
        OR designated_live_close_runtime_index GREATER_EQUAL designated_live_detach_index
    )
        message(
            FATAL_ERROR
            "Designated live image stage zero must wait, post-child revalidate, "
            "close retained mount descriptors, and only then detach"
        )
    endif()
    string(
        FIND
        "${designated_live_macos_capsule_script_contents}"
        [=["-force"]=]
        designated_live_force_detach_index
    )
    if(NOT designated_live_force_detach_index EQUAL -1)
        message(FATAL_ERROR "Designated live image stage zero must never force detach")
    endif()
    # BPR-0836. `hdiutil create` can exit 0 with its write-once device still
    # attached, so the image build detaches its own image, or refuses, before
    # it hashes and reports it.
    string(
        REGEX MATCHALL
        "detach_created_image[(]arguments[.]output, arguments[.]hdiutil[)]"
        designated_live_created_image_detach_calls
        "${designated_live_macos_capsule_script_compact}"
    )
    list(
        LENGTH
        designated_live_created_image_detach_calls
        designated_live_created_image_detach_call_count
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "def build_image("
        designated_live_build_image_index
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "detach_created_image(arguments.output, arguments.hdiutil)"
        designated_live_created_image_detach_index
    )
    string(
        FIND
        "${designated_live_macos_capsule_script_compact}"
        "digest = image_sha256(arguments.output)"
        designated_live_image_digest_index
    )
    if(
        NOT designated_live_created_image_detach_call_count EQUAL 1
        OR designated_live_build_image_index EQUAL -1
        OR designated_live_image_digest_index EQUAL -1
        OR designated_live_build_image_index GREATER_EQUAL designated_live_created_image_detach_index
        OR designated_live_created_image_detach_index GREATER_EQUAL designated_live_image_digest_index
    )
        message(
            FATAL_ERROR
            "Designated live image build must detach its own created image "
            "exactly once before hashing it"
        )
    endif()
    foreach(
        forbidden_stage_one_fragment
        IN ITEMS
            [=[subprocess.run(arguments]=]
            [=[signal.pthread_sigmask]=]
    )
        string(
            FIND
            "${designated_live_macos_capsule_script_contents}"
            "${forbidden_stage_one_fragment}"
            designated_live_forbidden_stage_one_index
        )
        if(NOT designated_live_forbidden_stage_one_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live stage one must own a bounded, signal-safe process group"
            )
        endif()
    endforeach()
endif()

if(EXISTS "${designated_live_stage_zero_preloader_file}")
    file(
        READ
        "${designated_live_stage_zero_preloader_file}"
        designated_live_stage_zero_preloader_contents
    )
    foreach(
        required_preloader_fragment
        IN ITEMS
            [=["/usr/bin/python3"]=]
            [=[os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW]=]
            [=[before = os.fstat(descriptor)]=]
            [=[after = os.fstat(descriptor)]=]
            [=[before.st_size > 1048576]=]
            [=[identity(before) != identity(after)]=]
            [=[hashlib.sha256(captured).hexdigest() != expected]=]
            [=[exec(compile(captured, path, "exec"), scope, scope)]=]
    )
        string(
            FIND
            "${designated_live_stage_zero_preloader_contents}"
            "${required_preloader_fragment}"
            required_preloader_fragment_index
        )
        if(required_preloader_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live stage-zero protected loader is incomplete: "
                "${required_preloader_fragment}"
            )
        endif()
    endforeach()
endif()

if(EXISTS "${designated_live_test_isolation_file}")
    file(
        READ
        "${designated_live_test_isolation_file}"
        designated_live_test_isolation_contents
    )
    foreach(
        required_live_image_fragment
        IN ITEMS
            [=[BPR_DESIGNATED_LIVE_MUTATION_IMAGE]=]
            [=[BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256]=]
            [=[BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT]=]
            [=[BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR]=]
            [=[source-manifest.txt]=]
            [=[bpr_designated_live_stage_zero_sha256]=]
            [=[bpr_designated_live_stage_zero_command(]=]
            [=[EXPECTED_IMAGE_SHA256 "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256}"]=]
            [=[COMMAND ${bpr_designated_live_attended_command}]=]
    )
        string(
            FIND
            "${designated_live_test_isolation_contents}"
            "${required_live_image_fragment}"
            required_live_image_fragment_index
        )
        if(required_live_image_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live CTest image registration is incomplete: "
                "${required_live_image_fragment}"
            )
        endif()
    endforeach()
    string(
        REGEX REPLACE
        "[ \t\r\n]+"
        " "
        designated_live_test_isolation_compact
        "${designated_live_test_isolation_contents}"
    )
    foreach(
        required_manifest_capture_fragment
        IN ITEMS
            [=[file( READ "${bpr_designated_live_source_manifest}" bpr_designated_live_source_manifest_bytes LIMIT 1048577 )]=]
            [=[string( SHA256 bpr_designated_live_source_manifest_sha256 "${bpr_designated_live_source_manifest_bytes}" )]=]
            [=[string( SUBSTRING "${bpr_designated_live_source_manifest_bytes}" "${bpr_designated_live_source_manifest_last}" 1 bpr_designated_live_source_manifest_terminator )]=]
            [=[string( SUBSTRING "${bpr_designated_live_source_manifest_bytes}" 0 "${bpr_designated_live_source_manifest_last}" bpr_designated_live_source_manifest_body )]=]
            [=[string( REPLACE "\n" ";" bpr_designated_live_source_entries "${bpr_designated_live_source_manifest_body}" )]=]
    )
        string(
            FIND
            "${designated_live_test_isolation_compact}"
            "${required_manifest_capture_fragment}"
            required_manifest_capture_fragment_index
        )
        if(required_manifest_capture_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live source manifest must hash and parse one bounded capture: "
                "${required_manifest_capture_fragment}"
            )
        endif()
    endforeach()
    foreach(
        forbidden_manifest_reopen
        IN ITEMS
            [=[file( SHA256 "${bpr_designated_live_source_manifest}"]=]
            [=[file( STRINGS "${bpr_designated_live_source_manifest}"]=]
            [=[file( TIMESTAMP "${bpr_designated_live_source_manifest}"]=]
    )
        string(
            FIND
            "${designated_live_test_isolation_compact}"
            "${forbidden_manifest_reopen}"
            forbidden_manifest_reopen_index
        )
        if(NOT forbidden_manifest_reopen_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live source manifest must not be reopened after capture"
            )
        endif()
    endforeach()
    foreach(
        forbidden_plain_live_command
        IN ITEMS
            [=[COMMAND ${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}]=]
            [=[COMMAND ${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}]=]
    )
        string(
            FIND
            "${designated_live_test_isolation_compact}"
            "${forbidden_plain_live_command}"
            forbidden_plain_live_command_index
        )
        if(NOT forbidden_plain_live_command_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live CTest must not register a plain supervisor or inventory"
            )
        endif()
    endforeach()
endif()

# BPR-0262 fact_4, BPR-0351. Attended authority is refused unless the exact
# bound supervisor launched this process, and that has to be settled before
# QApplication exists. The wiring is production-only -- a tests-enabled build
# rejects the approval flag outright -- so no tests-enabled runtime can observe
# it, and source order is what pins it.
get_filename_component(
    app_entry_point_file
    "${BPR_ROOT}/${app_entry_point_relative}"
    ABSOLUTE
)
if(EXISTS "${app_entry_point_file}")
    file(READ "${app_entry_point_file}" app_entry_point_contents)
    string(
        FIND
        "${app_entry_point_contents}"
        "designated_live_provenance::authenticate_supervisor("
        app_provenance_index
    )
    string(
        FIND
        "${app_entry_point_contents}"
        "QApplication qtApplication{ argc, argv };"
        app_qapplication_index
    )
    if(app_provenance_index EQUAL -1)
        message(
            FATAL_ERROR
            "The application must authenticate its bound supervisor before "
            "accepting attended authority"
        )
    endif()
    if(app_qapplication_index EQUAL -1)
        message(
            FATAL_ERROR
            "The application entry point no longer constructs QApplication "
            "where this check can see it"
        )
    endif()
    if(app_provenance_index GREATER_EQUAL app_qapplication_index)
        message(
            FATAL_ERROR
            "Supervisor provenance must be established before QApplication "
            "and before any public observation"
        )
    endif()
endif()

# fact_3 forbids every cheaper channel. The provenance component may consult
# the inherited port right and nothing else; a fallback here would invalidate
# the recorded approval rather than merely weaken it.
get_filename_component(
    app_designated_live_provenance_file
    "${BPR_ROOT}/${app_designated_live_provenance_relative}"
    ABSOLUTE
)
if(EXISTS "${app_designated_live_provenance_file}")
    file(
        READ
        "${app_designated_live_provenance_file}"
        app_designated_live_provenance_contents
    )
    foreach(
        forbidden_provenance_pattern
        IN ITEMS
            "(^|[^A-Za-z0-9_])(getenv|secure_getenv|qgetenv|environ)([^A-Za-z0-9_]|$)"
            "(^|[^A-Za-z0-9_])getppid([^A-Za-z0-9_]|$)"
            "#[ \t]*include[ \t]*[<\"]Q"
    )
        if(
            app_designated_live_provenance_contents
                MATCHES "${forbidden_provenance_pattern}"
        )
            message(
                FATAL_ERROR
                "Supervisor provenance must not fall back to the environment, "
                "a parent pid, or Qt: ${forbidden_provenance_pattern}"
            )
        endif()
    endforeach()
    foreach(
        required_provenance_fragment
        IN ITEMS
            [=[kSecGuestAttributeAudit]=]
            [=[SecCodeCheckValidity]=]
            [=[MACH_RCV_TRAILER_AUDIT]=]
            [=[mach_ports_register]=]
    )
        string(
            FIND
            "${app_designated_live_provenance_contents}"
            "${required_provenance_fragment}"
            required_provenance_fragment_index
        )
        if(required_provenance_fragment_index EQUAL -1)
            message(
                FATAL_ERROR
                "Supervisor provenance policy is incomplete: "
                "${required_provenance_fragment}"
            )
        endif()
    endforeach()
endif()
verify_effect_owner(
    "(libsecret/secret\\.h|org\\.freedesktop\\.(Secret|secrets)|(^|[^A-Za-z0-9_])secret_(collection|item|password|schema|service|value)_[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])Secret(Collection|Item|Schema|Service|Value)[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])SECRET_(COLLECTION|ITEM|SCHEMA|SERVICE|VALUE)_[A-Z0-9_]*)"
    "Linux Secret Service API"
    "${linux_secret_service_relative}"
)

get_filename_component(
    github_process_interface_file
    "${BPR_ROOT}/${github_process_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_process_runner_interface_file
    "${BPR_ROOT}/${github_process_runner_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_process_runner_file
    "${BPR_ROOT}/${github_process_runner_relative}"
    ABSOLUTE
)
get_filename_component(
    github_cli_transport_file
    "${BPR_ROOT}/${github_cli_transport_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_runner_file
    "${BPR_ROOT}/${git_process_runner_relative}"
    ABSOLUTE
)
get_filename_component(
    agent_profile_policy_file
    "${BPR_ROOT}/${agent_profile_policy_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_port_file
    "${BPR_ROOT}/${git_process_port_relative}"
    ABSOLUTE
)
get_filename_component(
    git_process_factory_file
    "${BPR_ROOT}/${git_process_factory_relative}"
    ABSOLUTE
)

# [[BPR-1436]]. The shared QProcess supervision. It is its own Qt Core static
# target that depends on no first-party layer, so neither runner gains an
# adapter-to-adapter edge. It launches nothing, reads no environment and builds
# no Windows descriptor, handle list or job: those stay with each runner and
# the platform header. What it does own is pinned here, because both runners'
# containment now rests on it: the process-group kill, the bounded kill-and-
# reap and background reap, abandonment, and the serialized lane's quarantine,
# watchdog and joined worker.
set(process_support_cmake "${BPR_ROOT}/src/adapters/process_support/CMakeLists.txt")
foreach(
    required_process_support_file
    IN ITEMS
        "${process_support_cmake}"
        "${BPR_ROOT}/${process_support_source_relative}"
        "${BPR_ROOT}/${process_support_header_relative}"
)
    if(NOT EXISTS "${required_process_support_file}")
        message(
            FATAL_ERROR
            "Shared process supervision is missing: "
            "${required_process_support_file}"
        )
    endif()
endforeach()
file(READ "${process_support_cmake}" process_support_cmake_contents)
if(
    NOT process_support_cmake_contents
        MATCHES "add_library\\([ \t\r\n]*better_pull_review_process_support[ \t\r\n]+STATIC"
    OR NOT process_support_cmake_contents
        MATCHES "add_library\\([ \t\r\n]*BetterPullReview::ProcessSupport[ \t\r\n]+ALIAS[ \t\r\n]+better_pull_review_process_support[ \t\r\n]*\\)"
)
    message(
        FATAL_ERROR
        "Shared process supervision must be the static "
        "better_pull_review_process_support target"
    )
endif()
string(
    REPLACE
    "BetterPullReview::ProcessSupport"
    ""
    process_support_cmake_dependencies
    "${process_support_cmake_contents}"
)
if(
    process_support_cmake_dependencies
        MATCHES "(BetterPullReview::|better_pull_review_(core|application|adapter|platform)|Qt6::(Network|Gui|Widgets)|glaze|quill|sqlite)"
)
    message(
        FATAL_ERROR
        "Shared process supervision must depend on Qt Core only, not on a "
        "first-party layer, an adapter or a provider"
    )
endif()
file(READ "${BPR_ROOT}/${process_support_source_relative}" process_support_source_contents)
file(READ "${BPR_ROOT}/${process_support_header_relative}" process_support_header_contents)
set(
    process_support_contents
    "${process_support_header_contents}\n${process_support_source_contents}"
)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    process_support_compact
    "${process_support_contents}"
)
if(
    process_support_contents
        MATCHES "#[ \t]*include[ \t]*[<\"](bpr/(core|application|app)/|bpr/adapters/(github|git|agent|sqlite|qt|mcp|workspace|update)/|bpr/platform/)"
)
    message(
        FATAL_ERROR
        "Shared process supervision must not include a first-party layer, an "
        "adapter or the platform target"
    )
endif()
if(
    process_support_contents
        MATCHES "(setProgram|setArguments|setProcessEnvironment|QProcessEnvironment|setCreateProcessArgumentsModifier|setChildProcessModifier|setUnixProcessParameters|startDetached|QProcess::execute|(->|\\.)start[ \t\r\n]*\\(|CreateJobObject|PROC_THREAD_ATTRIBUTE_HANDLE_LIST|InitializeAcl|qgetenv|qEnvironmentVariable|getenv|\\.detach[ \t\r\n]*\\(|Worker\\.release[ \t\r\n]*\\(|new[ \t\r\n]+serialized_process_lane)"
)
    message(
        FATAL_ERROR
        "Forbidden launch, environment, Windows containment-construction, or "
        "detached-worker surface in shared process supervision"
    )
endif()
set(
    required_process_support_patterns
    "class[ \t]+serialized_process_lane[ \t]+final"
    "~serialized_process_lane[ \t\r\n]*\\("
    "ShutdownRequested\\.store[ \t\r\n]*\\("
    "Worker\\.join[ \t\r\n]*\\("
    "while[ \t\r\n]*\\([ \t\r\n]*Busy[ \t\r\n]*\\)"
    "Busy[ \t\r\n]*=[ \t\r\n]*true"
    "Pending[ \t\r\n]*=[ \t\r\n]*aItem"
    "WorkAvailable\\.notify_one[ \t\r\n]*\\("
    "const[ \t]+auto[ \t]+processGroup[ \t\r\n]*=[ \t\r\n]*-[ \t]*static_cast<pid_t>[ \t\r\n]*\\([ \t\r\n]*aProcessGroupId[ \t\r\n]*\\)"
    "::kill[ \t\r\n]*\\([ \t\r\n]*processGroup[ \t\r\n]*,[ \t\r\n]*SIGKILL[ \t\r\n]*\\)"
    "resolve_process_group[ \t\r\n]*\\([ \t\r\n]*const[ \t]+QProcess&[ \t\r\n]+aProcess[ \t\r\n]*,[ \t\r\n]*const[ \t]+qint64[ \t\r\n]+aKnownProcessGroupId[ \t\r\n]*\\)"
    "const[ \t]+auto[ \t]+processId[ \t\r\n]*=[ \t\r\n]*aProcess\\.processId[ \t\r\n]*\\([ \t]*\\)"
    "processId[ \t\r\n]*>[ \t\r\n]*1[ \t\r\n]*\\?[ \t\r\n]*processId[ \t\r\n]*:[ \t\r\n]*0"
    "aProcess\\.waitForFinished[ \t\r\n]*\\([ \t\r\n]*wait[ \t\r\n]*\\)"
    "aProcess\\.kill[ \t\r\n]*\\("
    "aProcess\\.release[ \t\r\n]*\\("
)
foreach(required_process_support_pattern IN LISTS required_process_support_patterns)
    if(NOT process_support_contents MATCHES "${required_process_support_pattern}")
        message(
            FATAL_ERROR
            "Shared process supervision must retain its process-group kill, "
            "group resolution, bounded reap and joined serialized worker: "
            "${required_process_support_pattern}"
        )
    endif()
endforeach()
string(
    REGEX MATCHALL
    "std::thread[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*"
    process_support_threads
    "${process_support_contents}"
)
list(LENGTH process_support_threads process_support_thread_count)
if(NOT process_support_thread_count EQUAL 1)
    message(
        FATAL_ERROR
        "Shared process supervision must own exactly one worker thread per lane"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "auto kill_process_tree\\(QProcess& aProcess, const qint64 aProcessGroupId\\) -> void \\{ kill_process_group\\(aProcessGroupId\\); if \\(aProcess\\.state\\(\\) != QProcess::NotRunning\\) \\{ aProcess\\.kill\\(\\); \\} \\}"
    OR NOT process_support_compact
        MATCHES
            "auto processGroupId = resolve_process_group\\(aProcess, aKnownProcessGroupId\\); aKillTree\\(aProcess, processGroupId\\); while \\(aProcess\\.state\\(\\) != QProcess::NotRunning\\) \\{ const auto wait = wait_milliseconds\\(deadline\\); if \\(wait == 0\\) \\{ return false; \\} static_cast<void>\\(aProcess\\.waitForFinished\\(wait\\)\\); processGroupId = resolve_process_group\\(aProcess, processGroupId\\); aKillTree\\(aProcess, processGroupId\\); \\}"
)
    message(
        FATAL_ERROR
        "Shared bounded reap must resolve the initial process group before "
        "initial and repeated kill attempts"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "auto finish_background_reap\\( QProcess& aProcess, const qint64 aKnownProcessGroupId, const std::chrono::milliseconds aTimeout, const process_tree_kill& aKillTree\\) -> bool \\{ const auto deadline = supervision_clock::now\\(\\) \\+ aTimeout; auto processGroupId = resolve_process_group\\(aProcess, aKnownProcessGroupId\\); while \\(aProcess\\.state\\(\\) != QProcess::NotRunning\\) \\{ const auto wait = wait_milliseconds\\(deadline\\); if \\(wait == 0\\) \\{ return false; \\} processGroupId = resolve_process_group\\(aProcess, processGroupId\\); aKillTree\\(aProcess, processGroupId\\);"
)
    message(
        FATAL_ERROR
        "Shared background reap must stop at its deadline and retain and "
        "refresh resolved process-group ownership throughout cleanup"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "if \\(execution->AwaitingReap != nullptr\\) \\{ \\{ std::lock_guard lock\\{ Mutex \\}; Quarantined = true; \\} SlotAvailable\\.notify_all\\(\\); \\} publish\\(item, std::move\\(execution->Result\\)\\);"
    OR NOT process_support_compact
        MATCHES
            "reaped = reap_in_background\\(\\*execution\\); if \\(reaped\\) \\{ execution->AwaitingReap\\.reset\\(\\); \\} else \\{ abandon_unreaped_process\\(execution->AwaitingReap\\); \\} \\} \\{ std::lock_guard lock\\{ Mutex \\}; Quarantined = !reaped; Busy = false; \\}"
)
    message(
        FATAL_ERROR
        "Shared process lane must publish reap_failure only after quarantine "
        "owns the process, then release the slot only after reap, and stay "
        "quarantined after abandoning a child"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "if \\(Quarantined\\) \\{ return TTraits::failure\\(lane_failure::reap_failure\\); \\} Busy = true; Pending = aItem;"
)
    message(
        FATAL_ERROR
        "Shared process lane must refuse every submission while an abandoned "
        "child keeps it quarantined"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "if \\( !aItem->Completed\\.wait_until\\( resultLock, completionDeadline, \\[&\\] \\{ return aItem->Result\\.has_value\\(\\); \\}\\)\\) \\{ \\{ std::lock_guard lock\\{ Mutex \\}; if \\(Busy\\) \\{ Quarantined = true; \\} \\} SlotAvailable\\.notify_all\\(\\); return TTraits::failure\\(lane_failure::reap_failure\\); \\}"
)
    message(
        FATAL_ERROR
        "Shared process lane completion watchdog must predicate-check "
        "publication and quarantine the busy worker before returning reap_failure"
    )
endif()
if(
    NOT process_support_compact
        MATCHES
            "catch \\(\\.\\.\\.\\) \\{ publish\\( item, TTraits::failure\\(lane_failure::start_failure\\)\\); \\{ std::lock_guard lock\\{ Mutex \\}; Quarantined = false; Busy = false; \\} SlotAvailable\\.notify_all\\(\\); continue; \\}"
)
    message(
        FATAL_ERROR
        "Shared process lane exception recovery must clear any watchdog "
        "quarantine before reopening the serialized slot"
    )
endif()
foreach(process_support_runner_relative IN ITEMS "${github_process_runner_relative}" "${git_process_runner_relative}")
    if(EXISTS "${BPR_ROOT}/${process_support_runner_relative}")
        file(READ "${BPR_ROOT}/${process_support_runner_relative}" process_support_runner_contents)
        if(
            NOT process_support_runner_contents
                MATCHES "#[ \t]*include[ \t]*\"bpr/adapters/process_support/process_supervision\\.hpp\""
            OR NOT process_support_runner_contents
                MATCHES "process_support::serialized_process_lane<process_lane_traits>[ \t\r\n]+Lane;"
        )
            message(
                FATAL_ERROR
                "${process_support_runner_relative} must run its launches "
                "through the shared serialized process lane"
            )
        endif()
    endif()
endforeach()

file(READ "${git_process_runner_file}" git_process_runner_contents)
file(READ "${git_process_port_file}" git_process_port_contents)
file(READ "${git_process_factory_file}" git_process_factory_contents)
set(
    required_git_process_patterns
    "QProcess::SeparateChannels"
    "QProcess::ManagedInputChannel"
    "QProcess::UnixProcessFlag::CloseFileDescriptors"
    "QProcess::UnixProcessFlag::DisableCoreDumps"
    "QProcess::UnixProcessFlag::ResetSignalHandlers"
    "QProcess::UnixProcessFlag::CreateNewSession"
    "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE"
    "class[ \t]+process_worker[ \t]+final"
    # [[BPR-1436]]. The lane's quarantine and abandonment are the shared
    # supervision's, pinned with it above; this runner's bounded background
    # reap hands its child to them.
    "process_support::serialized_process_lane<process_lane_traits>[ \t\r\n]+Lane"
    "process_support::finish_background_reap"
    "empty_private_directory"
    # Both spellings, as every other cancellation pattern in this file already
    # accepts. Until BPR-0155 this one required the bare `stop.`, and the only
    # line in the runner that still spelled it that way was inside
    # `#elif defined(Q_OS_WIN)`. The contract's evidence that the runner honours
    # cancellation was a line no compiler on this host built.
    "(stop|aStop)\\.stop_requested\\(\\)"
    "maximum_pending_input_bytes"
    "bufferedInputOffset"
    "class[ \t]+private_input_file[ \t]+final"
    "O_NOFOLLOW"
    "FILE_FLAG_OPEN_REPARSE_POINT"
    "standard_output_limit"
    "standard_error_limit"
    "\"GIT_CONFIG_NOSYSTEM\""
    "\"GIT_CONFIG_SYSTEM\""
    "\"GIT_CONFIG_GLOBAL\""
    "\"GIT_ATTR_NOSYSTEM\""
    "\"GIT_EXEC_PATH\""
    "\"GIT_TEMPLATE_DIR\""
    "\"GIT_TERMINAL_PROMPT\""
    "\"GIT_NO_REPLACE_OBJECTS\""
    "\"GIT_NO_LAZY_FETCH\""
    "\"GIT_OPTIONAL_LOCKS\""
    "\"HOME\""
    "\"XDG_CONFIG_HOME\""
    "\"PATH\""
    "\"core\\.fsync=all\""
    "\"core\\.fsyncMethod=fsync\""
    "\"core\\.hooksPath=\""
    "\"protocol\\.allow=never\""
    "\"--git-dir=\""
    "\"index-pack\""
    "\"--stdin\""
    # [[BPR-0880]] The strict path carries Git's own severity list, so the
    # option is spelled `--strict=<msg-id>=ignore`. The list itself is pinned
    # by its table marker below, and the sparse path's option is pinned here
    # too now that it is part of the same fixed contract.
    "\"--strict=\""
    "\"--fsck-objects\""
    "BPR_0880_FSCK_MESSAGE_TABLE_BEGIN"
    "\"=ignore\""
    "\"--check-self-contained-and-connected\""
    "\"--max-input-size=\""
    "\"--threads=1\""
    "\"--no-rev-index\""
    "\"cat-file\""
    "\"merge-base\""
    "\"update-ref\""
    "\"--no-deref\""
    "\"for-each-ref\""
    "transaction_success"
    "probe_pack_sha256"
    "capability_probe[ \t\r\n]*\\("
    # [[BPR-1115]]. Every unguarded Windows Git takes the platform target's
    # launch restriction, whose refusal creates nothing, and a Git that started
    # without it all the same is refused. [[BPR-1265]]. The restriction also
    # names the kill-on-close job, so process creation puts Git in it, and a
    # Git found outside the job after the start is refused.
    "#[ \t]*include[ \t]*\"bpr/platform/windows_standard_handles\\.hpp\""
    "windows_standard_handles::launch_restriction"
    "(aInheritance|inheritance)\\.apply[ \t\r\n]*\\([ \t\r\n]*aArguments->startupInfo[ \t\r\n]*,[ \t\r\n]*aArguments->flags[ \t\r\n]*,[ \t\r\n]*aArguments->inheritHandles[ \t\r\n]*\\)"
    "restrict_to_standard_handles[ \t\r\n]*\\([ \t\r\n]*\\*process"
    "inheritance\\.place_in_job[ \t\r\n]*\\([ \t\r\n]*job\\.native[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*\\)"
    "!inheritance\\.placed_in_job[ \t\r\n]*\\([ \t]*\\)"
    "IsProcessInJob[ \t\r\n]*\\("
)
foreach(required_git_process_pattern IN LISTS required_git_process_patterns)
    if(
        NOT git_process_runner_contents
            MATCHES "${required_git_process_pattern}"
    )
        message(
            FATAL_ERROR
            "Installed-Git runner contract is incomplete: "
            "${required_git_process_pattern}"
        )
    endif()
endforeach()

set(
    forbidden_git_process_patterns
    # [[BPR-1265]]. Assigning a started Git to its job by process id raced a
    # Git that had already exited; the job is named at creation instead.
    "AssignProcessToJobObject"
    "QNetwork[A-Za-z0-9_]*"
    "QProcessEnvironment::systemEnvironment"
    "QProcessEnvironment::InheritFromParent"
    "(^|[^A-Za-z0-9_])(qgetenv|qEnvironmentVariable|getenv|secure_getenv|environ)([^A-Za-z0-9_]|$)"
    "(startCommand|splitCommand|setNativeArguments|startDetached|QProcess::execute)"
    "\"(fetch-pack|upload-pack|unpack-objects|clone|remote|submodule|checkout|merge|diff)\""
    "\"--(fix-thin|keep)\""
    "https?://"
    "BPR_[A-Z0-9_]*TEST"
)
foreach(forbidden_git_process_pattern IN LISTS forbidden_git_process_patterns)
    if(
        git_process_runner_contents
            MATCHES "${forbidden_git_process_pattern}"
    )
        message(
            FATAL_ERROR
            "Forbidden installed-Git process surface: "
            "${forbidden_git_process_pattern}"
        )
    endif()
endforeach()

# The Git application port may carry bounded, redacted diagnostic text.
#
# This rule used to forbid `std::string` outright, alongside `std::vector`. That
# cost real diagnosis: a failing `git` subprocess explains itself on standard
# error, the port had nowhere to put that explanation, and BPR-0341 needed an
# instrumented build to recover text the process had already printed.
#
# Two architecture documents argue that a destination should carry nothing —
# `docs/architecture/review-queue.md` for the operating system's notification
# store, and `docs/architecture/ai-agent-boundary.md` for the supplement sent to
# a third-party agent. Both are about **repository content leaving the product**:
# a notification store outlives the application and may sync to other devices, and
# an AI supplement goes to a tool the reviewer did not write. Neither describes
# this port. A diagnostic returned from an adapter to the application that invoked
# it does not leave the product, is read by the operator who caused it, and is
# already the standing instruction for subprocesses — see
# `docs/architecture/security-threat-model.md`, "Use bounded stdin/stdout/stderr,
# timeouts, cancellation, and redaction."
#
# So the rule is narrowed rather than removed, and what it still forbids is
# stated below. Product-owner ruling on BPR-0348, 2026-08-19.
set(
    forbidden_git_public_port_patterns
    "#[ \t]*include[ \t]*[<\"]Q"
    "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
    "(Arguments|Environment|WorkingDirectory|Timeout|OutputLimit)"
    "(URL|Url|URI|Credential|Authorization|Proxy|Certificate)"
    "std::vector"
)
foreach(forbidden_pattern IN LISTS forbidden_git_public_port_patterns)
    if(git_process_port_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Git application port exposes forbidden process/provider "
            "authority: ${forbidden_pattern}"
        )
    endif()
endforeach()

# `std::vector` stays forbidden above, because an argument or environment list is
# authority rather than diagnosis. Dropping `std::string` from that same
# alternation would have admitted both in one edit, which is why they are now
# separate: text is permitted, collections are not.
#
# Permitting text is not permitting arbitrary text. The port may carry exactly one
# diagnostic member, so a second string cannot arrive unnoticed and be argued
# about afterwards. The named-authority patterns above still apply to it, so a
# `std::string Url` or `std::string Credential` is rejected by name regardless of
# this allowance.
#
# Counted rather than matched line by line: a declaration contains semicolons,
# `string(REGEX MATCHALL)` yields a CMake list, and a list splits on `;` — so
# iterating "lines" would hand this loop the fragment `struct git_process_error {
# int Code` and reject the very shape the rule exists to permit. Counting the two
# forms sidesteps that, because neither match can contain a semicolon.
string(
    REGEX MATCHALL "std::string"
    git_process_port_text_uses "${git_process_port_contents}"
)
string(
    REGEX MATCHALL "std::string[ \t]+Detail"
    git_process_port_permitted_text_uses "${git_process_port_contents}"
)
list(LENGTH git_process_port_text_uses git_process_port_text_count)
list(
    LENGTH
    git_process_port_permitted_text_uses
    git_process_port_permitted_text_count
)
if(NOT git_process_port_text_count EQUAL git_process_port_permitted_text_count)
    message(
        FATAL_ERROR
        "Git application port carries text outside its one permitted "
        "diagnostic member 'Detail'"
    )
endif()
if(git_process_port_permitted_text_count GREATER 1)
    message(
        FATAL_ERROR
        "Git application port declares more than one diagnostic member 'Detail'"
    )
endif()
if(
    git_process_factory_contents MATCHES "#[ \t]*include[ \t]*[<\"]Q"
    OR git_process_factory_contents MATCHES "BPR_[A-Z0-9_]*TEST"
)
    message(
        FATAL_ERROR
        "Installed-Git factory header must remain standard C++ and "
        "production-only"
    )
endif()

foreach(
    standard_process_header
    IN ITEMS
        "${github_process_interface_file}"
        "${github_process_runner_interface_file}"
)
    file(READ "${standard_process_header}" standard_process_header_contents)
    set(
        forbidden_standard_process_header_patterns
        "#[ \t]*include[ \t]*[<\"]Q"
        "#[ \t]*include[ \t]*[<\"]Qt"
        "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
        "#[ \t]*include[ \t]*[<\"](windows\\.h|process\\.h|spawn\\.h|sys/wait\\.h)"
        "(^|[^A-Za-z0-9_])(pid_t|HANDLE|PROCESS_INFORMATION|STARTUPINFO[A-Z]?|FILE)([^A-Za-z0-9_]|$)"
        "std::string[ \t\r\n]+StandardError([^A-Za-z0-9_]|$)"
        "(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN)([^A-Za-z0-9_]|$)"
    )
    foreach(
        forbidden_standard_process_header_pattern
        IN LISTS forbidden_standard_process_header_patterns
    )
        if(
            standard_process_header_contents
                MATCHES "${forbidden_standard_process_header_pattern}"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${standard_process_header}"
            )
            message(
                FATAL_ERROR
                "GitHub CLI process interface must remain standard C++ and "
                "effect-free in ${relative_file}: "
                "${forbidden_standard_process_header_pattern}"
            )
        endif()
    endforeach()
endforeach()

file(
    READ
    "${github_process_runner_interface_file}"
    github_process_runner_interface_contents
)
file(READ "${github_process_runner_file}" github_process_runner_contents)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    github_process_runner_compact
    "${github_process_runner_contents}"
)

string(
    FIND
    "${github_process_runner_compact}"
    "auto contains_nul("
    github_request_shape_start
)
string(
    FIND
    "${github_process_runner_compact}"
    "[[nodiscard]] auto decode_utf8("
    github_request_shape_end
)
if(
    github_request_shape_start EQUAL -1
    OR github_request_shape_end EQUAL -1
    OR NOT github_request_shape_start LESS github_request_shape_end
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must define bounded allocation-free "
        "pre-copy helper and request-shape validation"
    )
endif()
math(
    EXPR
    github_request_shape_length
    "${github_request_shape_end} - ${github_request_shape_start}"
)
string(
    SUBSTRING
    "${github_process_runner_compact}"
    "${github_request_shape_start}"
    "${github_request_shape_length}"
    github_request_shape_contents
)
set(
    required_github_request_shape_patterns
    "valid_request_shape\\( const github_cli_process_request& (request|aRequest)\\) noexcept -> bool"
    "(request|aRequest)\\.Executable\\.empty\\(\\)"
    "(request|aRequest)\\.Executable\\.size\\(\\) > github_cli_process_max_path_bytes"
    "contains_nul\\((request|aRequest)\\.Executable\\)"
    "(request|aRequest)\\.WorkingDirectory\\.empty\\(\\)"
    "(request|aRequest)\\.WorkingDirectory\\.size\\(\\) > github_cli_process_max_path_bytes"
    "contains_nul\\((request|aRequest)\\.WorkingDirectory\\)"
    "(request|aRequest)\\.Arguments\\.empty\\(\\)"
    "(request|aRequest)\\.Arguments\\.size\\(\\) > github_cli_process_max_argument_count"
    "(request|aRequest)\\.Environment\\.size\\(\\) > github_cli_process_max_environment_count"
    "(request|aRequest)\\.StandardInputLimit == 0"
    "(request|aRequest)\\.StandardInputLimit > github_cli_process_max_standard_input_bytes"
    "(request|aRequest)\\.StandardInput\\.size\\(\\) > (request|aRequest)\\.StandardInputLimit"
    "(request|aRequest)\\.StandardOutputLimit == 0"
    "(request|aRequest)\\.StandardOutputLimit > github_cli_process_max_standard_output_bytes"
    "(request|aRequest)\\.StandardErrorLimit == 0"
    "(request|aRequest)\\.StandardErrorLimit > github_cli_process_max_standard_error_bytes"
    "(request|aRequest)\\.Timeout <= std::chrono::milliseconds::zero\\(\\)"
    "(request|aRequest)\\.Timeout > std::chrono::duration_cast<std::chrono::milliseconds>\\( github_cli_process_max_timeout\\)"
    "(request|aRequest)\\.ReapTimeout <= std::chrono::milliseconds::zero\\(\\)"
    "(request|aRequest)\\.ReapTimeout > std::chrono::duration_cast<std::chrono::milliseconds>\\( github_cli_process_max_reap_timeout\\)"
    "argument\\.empty\\(\\)"
    "argument\\.size\\(\\) > github_cli_process_max_argument_bytes"
    "contains_nul\\(argument\\)"
    "add_bounded\\( (argument_bytes|argumentBytes), argument\\.size\\(\\), github_cli_process_max_arguments_bytes\\)"
    "entry\\.Name\\.size\\(\\) > github_cli_process_max_environment_name_bytes"
    "valid_environment_name\\(entry\\.Name\\)"
    "entry\\.Value\\.size\\(\\) > github_cli_process_max_environment_value_bytes"
    "contains_nul\\(entry\\.Value\\)"
    "add_bounded\\( (environment_bytes|environmentBytes), entry\\.Name\\.size\\(\\), github_cli_process_max_environment_bytes\\)"
    "add_bounded\\( (environment_bytes|environmentBytes), entry\\.Value\\.size\\(\\), github_cli_process_max_environment_bytes\\)"
    "equal_environment_names\\( entry\\.Name, (request|aRequest)\\.Environment\\[prior\\]\\.Name\\)"
    "!(has_empty_path|hasEmptyPath) \\|\\| !(has_system_root|hasSystemRoot)"
)
foreach(
    required_request_shape_pattern
    IN LISTS required_github_request_shape_patterns
)
    if(
        NOT github_request_shape_contents
            MATCHES "${required_request_shape_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI pre-copy request-shape validation must retain all "
            "path, argument, environment, stream, and deadline bounds: "
            "${required_request_shape_pattern}"
        )
    endif()
endforeach()
set(
    forbidden_github_request_shape_patterns
    "(QString|QByteArray|QFileInfo|QDir|decode_utf8)"
    "std::(string|vector|deque|list|map|unordered_[a-z_]+|set)([^A-Za-z0-9_]|$)"
    "std::(make_unique|make_shared|allocate_shared)[ \t]*[<(]"
    "(^|[^A-Za-z0-9_])(new|malloc|calloc|realloc|aligned_alloc)[ \t\r\n]*(\\(|[A-Za-z_])"
    "\\.(reserve|resize|assign|append|push_back|emplace|emplace_back)[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])(try|throw)([^A-Za-z0-9_]|$)"
)
foreach(
    forbidden_request_shape_pattern
    IN LISTS forbidden_github_request_shape_patterns
)
    if(
        github_request_shape_contents
            MATCHES "${forbidden_request_shape_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI pre-copy request-shape validation must remain "
            "allocation-free and non-throwing: "
            "${forbidden_request_shape_pattern}"
        )
    endif()
endforeach()
if(
    NOT github_process_runner_compact
        MATCHES
            "if[ ]*\\(total > limit \\|\\| amount > limit - total\\) \\{ return false; \\}"
    AND NOT github_process_runner_compact
        MATCHES
            "if[ ]*\\(aTotal > aLimit \\|\\| aAmount > aLimit - aTotal\\) \\{ return false; \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI aggregate pre-copy bounds must reject overflow before "
        "subtraction"
    )
endif()
if(
    NOT github_process_runner_compact
        MATCHES
            "if[ ]*\\(!valid_request_shape\\((request|aRequest)\\)\\).*github_cli_process_failure::invalid_request.*if[ ]*\\((stop|aStop)\\.stop_requested\\(\\)\\).*github_cli_process_failure::cancelled.*std::shared_ptr<process_work_item> item; try \\{ item = std::make_shared<process_work_item>\\(\\); item->Request = (request|aRequest);"
)
    message(
        FATAL_ERROR
        "GitHub CLI worker must validate request shape and cancellation "
        "before allocating or copying the work item"
    )
endif()
string(
    REGEX MATCHALL
    "item->Request = (request|aRequest)"
    github_request_work_item_copies
    "${github_process_runner_compact}"
)
list(LENGTH github_request_work_item_copies github_request_work_item_copy_count)
if(NOT github_request_work_item_copy_count EQUAL 1)
    message(
        FATAL_ERROR
        "GitHub CLI worker must have exactly one guarded request-copy site"
    )
endif()

set(
    required_github_process_runner_patterns
    "#[ \t]*include[ \t]*[<\"]QProcess[>\"]"
    "#[ \t]*include[ \t]*[<\"]QProcessEnvironment[>\"]"
    "setProgram[ \t\r\n]*\\("
    "setArguments[ \t\r\n]*\\("
    "setProcessEnvironment[ \t\r\n]*\\("
    "setWorkingDirectory[ \t\r\n]*\\("
    "\"\\.git\""
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"PATH\"[ \t\r\n]*\\)"
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"SystemRoot\"[ \t\r\n]*\\)"
    "QProcess::SeparateChannels"
    "QProcess::ManagedInputChannel"
    "write[ \t\r\n]*\\("
    "closeWriteChannel[ \t\r\n]*\\("
    "setReadChannel[ \t\r\n]*\\("
    "read[ \t\r\n]*\\("
    "waitForStarted[ \t\r\n]*\\("
    "waitForReadyRead[ \t\r\n]*\\("
    "std::make_unique<QProcess>[ \t\r\n]*\\("
    "(^|[^A-Za-z0-9_])StandardInputLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])StandardOutputLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])StandardErrorLimit([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])Timeout([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])ReapTimeout([^A-Za-z0-9_]|$)"
)
foreach(required_runner_pattern IN LISTS required_github_process_runner_patterns)
    if(NOT github_process_runner_contents MATCHES "${required_runner_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain explicit program, "
            "argument, empty-environment, neutral-directory, separate-stream, "
            "bounded-I/O, timeout, kill, and reap handling: "
            "${required_runner_pattern}"
        )
    endif()
endforeach()

set(
    required_github_process_windows_environment_patterns
    "#if[ \t]+defined[ \t]*\\([ \t]*_WIN32[ \t]*\\)"
    "bool[ \t]+(has_empty_path|hasEmptyPath)[ \t]*=[ \t]*false"
    "bool[ \t]+(has_system_root|hasSystemRoot)[ \t]*=[ \t]*false"
    "(has_empty_path|hasEmptyPath)[ \t]*=[ \t]*entry\\.Value\\.empty[ \t\r\n]*\\([ \t]*\\)"
    "(has_system_root|hasSystemRoot)[ \t]*=[ \t]*![ \t]*entry\\.Value\\.empty[ \t\r\n]*\\([ \t]*\\)"
    "![ \t]*(has_empty_path|hasEmptyPath)[ \t]*\\|\\|[ \t]*![ \t]*(has_system_root|hasSystemRoot)"
)
foreach(
    required_windows_environment_pattern
    IN LISTS required_github_process_windows_environment_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_windows_environment_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain explicit Windows empty "
            "PATH and non-empty SystemRoot validation: "
            "${required_windows_environment_pattern}"
        )
    endif()
endforeach()

set(
    required_github_process_unix_containment_patterns
    "setUnixProcessParameters[ \t\r\n]*\\("
    "QProcess::UnixProcessFlag::CloseFileDescriptors"
    "QProcess::UnixProcessFlag::DisableCoreDumps"
    "QProcess::UnixProcessFlag::ResetSignalHandlers"
    "QProcess::UnixProcessFlag::CreateNewSession"
    "QProcess::started"
    "(process_pointer|processPointer)->processId[ \t\r\n]*\\("
    "kill_process_group_and_reap[ \t\r\n]*\\("
    "finish_background_reap[ \t\r\n]*\\("
)
foreach(
    required_unix_containment_pattern
    IN LISTS required_github_process_unix_containment_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_unix_containment_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain Unix child hardening and "
            "initial process-group kill/reap containment: "
            "${required_unix_containment_pattern}"
        )
    endif()
endforeach()

# [[BPR-1436]]. resolve_process_group, the bounded kill-and-reap and the
# background reap are the shared supervision's now, pinned with it.
set(
    required_process_group_start_resolution
    "process->start\\(\\); \\*(process_group_id|processGroupId) = resolve_process_group\\( \\*process, \\*(process_group_id|processGroupId)\\); for[ ]*\\(;;\\) \\{ \\*(process_group_id|processGroupId) = resolve_process_group\\( \\*process, \\*(process_group_id|processGroupId)\\); if[ ]*\\(const auto failure = (terminal_failure|terminalFailure)\\(\\); failure\\.has_value\\(\\)\\)"
)
if(
    NOT github_process_runner_compact
        MATCHES "${required_process_group_start_resolution}"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must resolve the initial process group "
        "immediately after start and before every startup-loop terminal check"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "process_support::finish_background_reap\\( \\*aExecution\\.AwaitingReap, aExecution\\.ProcessGroupId, github_cli_process_max_reap_timeout, tree_kill_for\\(TestSeam\\)\\);"
)
    message(
        FATAL_ERROR
        "GitHub CLI background reap must be the shared bounded reap, given "
        "the longest reap a request may ask for"
    )
endif()

if(
    NOT github_process_runner_compact
        MATCHES
            "const auto (process_group_id|processGroupId) = resolve_process_group\\(\\*(process|aProcess), (known_process_group_id|aKnownProcessGroupId)\\); if[ ]*\\( (process_support::)?kill_process_group_and_reap\\( \\*(process|aProcess), (process_group_id|processGroupId), (reap_timeout|aReapTimeout)(, (kill_tree|aKillTree))?\\)\\)"
)
    message(
        FATAL_ERROR
        "GitHub CLI failed execution must resolve initial process-group "
        "ownership before bounded reap or quarantine handoff"
    )
endif()

set(
    required_stable_process_group_state
    "const auto (process_group_id|processGroupId) = std::make_shared<qint64>\\(0\\);"
)
set(
    required_started_process_group_capture
    "QObject::connect\\( process\\.get\\(\\), &QProcess::started, process\\.get\\(\\), \\[[ ]*(process_pointer|processPointer) = process\\.get\\(\\), (process_group_id|processGroupId)[ ]*\\]\\(\\) noexcept \\{ \\*(process_group_id|processGroupId) = (process_pointer|processPointer)->processId\\(\\); \\}, Qt::DirectConnection\\);"
)
set(
    required_finished_process_group_cleanup
    "QObject::connect\\( process\\.get\\(\\), &QProcess::finished, process\\.get\\(\\), \\[[ ]*(process_group_id|processGroupId)[ ]*\\]\\( const int, const QProcess::ExitStatus\\) noexcept \\{ kill_process_group\\(\\*(process_group_id|processGroupId)\\); \\}, Qt::DirectConnection\\);"
)
if(
    NOT github_process_runner_compact
        MATCHES "${required_stable_process_group_state}"
    OR NOT github_process_runner_compact
        MATCHES "${required_started_process_group_capture}"
    OR NOT github_process_runner_compact
        MATCHES "${required_finished_process_group_cleanup}"
)
    message(
        FATAL_ERROR
        "GitHub CLI process signals must capture stable process-group state "
        "by value and synchronously kill that group on finish"
    )
endif()
string(
    FIND
    "${github_process_runner_compact}"
    "&QProcess::finished"
    github_process_finished_connection_position
)
string(
    FIND
    "${github_process_runner_compact}"
    "process->start();"
    github_process_start_position
)
if(
    github_process_finished_connection_position EQUAL -1
    OR github_process_start_position EQUAL -1
    OR NOT github_process_finished_connection_position
        LESS github_process_start_position
)
    message(
        FATAL_ERROR
        "GitHub CLI finished process-group cleanup must be connected before "
        "process start"
    )
endif()
if(
    NOT github_process_runner_compact
        MATCHES
            "kill_process_group\\(\\*(process_group_id|processGroupId)\\); return \\{ \\.Result = github_cli_process_success"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must repeat initial process-group cleanup "
        "immediately before publishing success"
    )
endif()

set(
    required_github_process_worker_patterns
    "class[ \t]+process_worker[ \t]+final"
    # [[BPR-1436]]. The joined worker, its slot and its quarantine are the
    # shared lane's, pinned with it; this worker owns one lane and submits the
    # item it built.
    "process_support::serialized_process_lane<process_lane_traits>[ \t\r\n]+Lane;"
    "Lane\\.submit[ \t\r\n]*\\([ \t\r\n]*item[ \t\r\n]*,[ \t\r\n]*(request|aRequest)\\.ReapTimeout[ \t\r\n]*\\)"
    "item->Deadline[ \t\r\n]*=[ \t\r\n]*clock::now[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*\\+[ \t\r\n]*(request|aRequest)\\.Timeout"
    "std::unique_ptr<process_worker>[ \t\r\n]+Worker"
    "Worker[ \t\r\n]*=[ \t\r\n]*std::make_unique<process_worker>[ \t\r\n]*\\("
    "std::mutex[ \t\r\n]+WorkerMutex"
)
foreach(required_worker_pattern IN LISTS required_github_process_worker_patterns)
    if(NOT github_process_runner_contents MATCHES "${required_worker_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain its runner-owned, "
            "serialized, joined worker lifecycle: "
            "${required_worker_pattern}"
        )
    endif()
endforeach()
string(
    REGEX MATCHALL
    "std::unique_ptr<process_worker>"
    github_process_worker_owners
    "${github_process_runner_contents}"
)
list(LENGTH github_process_worker_owners github_process_worker_owner_count)
string(
    REGEX MATCHALL
    "std::make_unique<process_worker>[ \t\r\n]*\\("
    github_process_worker_constructions
    "${github_process_runner_contents}"
)
list(
    LENGTH
    github_process_worker_constructions
    github_process_worker_construction_count
)
string(
    REGEX MATCHALL
    "std::thread[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*"
    github_process_worker_threads
    "${github_process_runner_contents}"
)
list(LENGTH github_process_worker_threads github_process_worker_thread_count)
# [[BPR-0807]] RFC-BPR-0015's Git acquisition gives a runner file limits. They
# arrive through a second production factory, not a parameter on the nullary
# one. That factory takes only the limits and passes no seam, and the
# construction helper is declared once and called only by the three factories.
# Factory and spool guards are matched over code alone: comments are removed
# first, so a guard or call that exists only in a comment counts for nothing.
string(
    REGEX REPLACE
    "/\\*([^*]|\\*+[^*/])*\\*+/"
    " "
    github_process_runner_code
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "//[^\r\n]*"
    " "
    github_process_runner_code
    "${github_process_runner_code}"
)
string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    github_process_runner_code_compact
    "${github_process_runner_code}"
)
string(
    REGEX MATCHALL
    "make_qprocess_github_cli_process_runner ?\\("
    github_process_runner_constructions
    "${github_process_runner_code_compact}"
)
list(
    LENGTH
    github_process_runner_constructions
    github_process_runner_construction_count
)
if(
    github_process_runner_contents
        MATCHES
            "github_cli_process_runner_test_seam[ \t\r\n]*\\*"
    OR github_process_runner_interface_contents
        MATCHES
            "github_cli_process_runner_test_seam[ \t\r\n]*\\*"
    OR NOT github_process_runner_interface_contents
        MATCHES
            "make_github_cli_process_runner[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*->[ \t\r\n]*std::unique_ptr<github_cli_process_port>"
    OR NOT github_process_runner_interface_contents
        MATCHES
            "make_github_cli_process_runner_for_test[ \t\r\n]*\\([ \t\r\n]*std::shared_ptr<github_cli_process_runner_test_seam>[ \t\r\n]+(test_seam|aTestSeam)[ \t\r\n]*\\)"
    OR NOT github_process_runner_compact
        MATCHES
            "auto make_github_cli_process_runner\\(\\) -> std::unique_ptr<github_cli_process_port> \\{ return make_qprocess_github_cli_process_runner\\(nullptr\\); \\}"
    OR NOT github_process_runner_compact
        MATCHES
            "auto make_github_cli_process_runner_for_test\\( std::shared_ptr<github_cli_process_runner_test_seam> (test_seam|aTestSeam)\\) -> std::unique_ptr<github_cli_process_port> \\{ return make_qprocess_github_cli_process_runner\\( std::move\\((test_seam|aTestSeam)\\)\\); \\}"
    OR NOT github_process_runner_interface_contents
        MATCHES
            "make_github_cli_git_acquisition_process_runner[ \t\r\n]*\\([ \t\r\n]*github_cli_process_file_limits[ \t\r\n]+aFileLimits[ \t\r\n]*\\)[ \t\r\n]*->[ \t\r\n]*std::unique_ptr<github_cli_process_port>"
    OR NOT github_process_runner_compact
        MATCHES
            "auto make_github_cli_git_acquisition_process_runner\\( ?github_cli_process_file_limits aFileLimits\\) -> std::unique_ptr<github_cli_process_port> \\{"
    OR NOT github_process_runner_compact
        MATCHES
            "return make_qprocess_github_cli_process_runner\\( ?nullptr, std::move\\(aFileLimits\\)\\); \\}"
    OR NOT github_process_runner_construction_count EQUAL 4
)
    message(
        FATAL_ERROR
        "GitHub CLI production factory must remain nullary while the explicit "
        "test factory and asynchronous worker retain shared seam ownership, "
        "and the Git-acquisition factory must take only file limits and no seam"
    )
endif()

if(
    NOT github_process_worker_owner_count EQUAL 1
    OR NOT github_process_worker_construction_count EQUAL 1
    # [[BPR-1436]]. The worker thread is the shared lane's.
    OR NOT github_process_worker_thread_count EQUAL 0
    # [[BPR-0807]] RFC-BPR-0015's file limits are a value the runner hands its
    # worker. They own no worker, so the runner stays the only owner.
    OR NOT github_process_runner_compact
        MATCHES
            "private: std::shared_ptr<github_cli_process_runner_test_seam> TestSeam; github_cli_process_file_limits FileLimits; std::mutex WorkerMutex; std::unique_ptr<process_worker> Worker; \\};"
)
    message(
        FATAL_ERROR
        "GitHub CLI process worker must have exactly one owner, the concrete "
        "runner instance"
    )
endif()

set(
    required_pre_start_guard
    "if[ ]*\\( (shutdown_requested|aShutdownRequested)\\.load\\([^;]*\\) \\|\\| (stop|aStop)\\.stop_requested\\(\\)\\) \\{ return failed\\( github_cli_process_failure::cancelled\\); \\} if[ ]*\\(clock::now\\(\\) >= (deadline|aDeadline)\\) \\{ return failed\\(github_cli_process_failure::timeout\\); \\} process->start\\(\\);"
)
if(NOT github_process_runner_compact MATCHES "${required_pre_start_guard}")
    message(
        FATAL_ERROR
        "GitHub CLI process runner must check shutdown, cancellation, and "
        "the absolute deadline immediately before process start"
    )
endif()
string(
    REGEX MATCHALL
    "process->start[ \t\r\n]*\\("
    github_process_start_calls
    "${github_process_runner_contents}"
)
list(LENGTH github_process_start_calls github_process_start_call_count)
if(NOT github_process_start_call_count EQUAL 1)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must have exactly one guarded process "
        "start site"
    )
endif()

set(
    required_github_process_reap_ownership_patterns
    "std::unique_ptr<QProcess>[ \t\r\n]+AwaitingReap"
    "\\.AwaitingReap[ \t\r\n]*=[ \t\r\n]*std::move[ \t\r\n]*\\([ \t\r\n]*(process|aProcess)[ \t\r\n]*\\)"
    "process_support::finish_background_reap[ \t\r\n]*\\([ \t\r\n]*\\*aExecution\\.AwaitingReap"
)
foreach(
    required_reap_ownership_pattern
    IN LISTS required_github_process_reap_ownership_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_reap_ownership_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain quarantine ownership until "
            "a failed bounded reap completes: "
            "${required_reap_ownership_pattern}"
        )
    endif()
endforeach()
# [[BPR-1436]]. Quarantine before publication, release after reap, the
# completion watchdog and exception recovery are the shared lane's, pinned
# with it.
string(
    REGEX MATCHALL
    "github_cli_process_failure::reap_failure"
    github_process_reap_failure_sites
    "${github_process_runner_contents}"
)
list(
    LENGTH
    github_process_reap_failure_sites
    github_process_reap_failure_site_count
)
if(NOT github_process_reap_failure_site_count EQUAL 2)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must retain exactly the bounded-reap and "
        "shared-lane reap_failure sites"
    )
endif()

set(
    required_github_process_bounded_read_patterns
    "std::array<char,[ \t\r\n]*16U[ \t]*\\*[ \t]*1024U>[ \t\r\n]+buffer"
    "const[ \t]+auto[ \t]+remaining[ \t\r\n]*=[ \t\r\n]*(limit|aLimit)[ \t\r\n]*-[ \t\r\n]*(consumed|aConsumed)"
    "std::min<std::size_t>[ \t\r\n]*\\([ \t\r\n]*buffer\\.size[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*remaining[ \t\r\n]*\\+[ \t\r\n]*1U[ \t\r\n]*\\)"
    "(process|aProcess)\\.read[ \t\r\n]*\\([ \t\r\n]*buffer\\.data[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*static_cast<qint64>[ \t\r\n]*\\([ \t\r\n]*requested[ \t\r\n]*\\)[ \t\r\n]*\\)"
    "(byte_count|byteCount)[ \t\r\n]*>[ \t\r\n]*remaining"
    "(consumed|aConsumed)[ \t\r\n]*\\+=[ \t\r\n]*(byte_count|byteCount)"
)
foreach(
    required_bounded_read_pattern
    IN LISTS required_github_process_bounded_read_patterns
)
    if(
        NOT github_process_runner_contents
            MATCHES "${required_bounded_read_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner must retain bounded fixed-buffer "
            "remaining-plus-one channel reads: "
            "${required_bounded_read_pattern}"
        )
    endif()
endforeach()

function(reject_github_process_runner_surface forbidden_pattern surface_name)
    if(github_process_runner_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${github_process_runner_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_github_process_runner_surface(
    "(startCommand|splitCommand|setNativeArguments|startDetached|QProcess::execute)"
    "shell/raw-command"
)
# [[BPR-1114]] Every Windows launch through the runner inherits its three
# standard handles and nothing else. The runner's one process-creation modifier
# is the platform target's launch restriction, pinned whole over code alone, so
# a modifier in a comment counts for nothing: it hands the restriction the
# startup block, creation flags and inheritance flag and touches nothing else,
# and the only thing the test seam may do is withhold the standard error handle
# so that the list cannot be built. A launch whose list cannot be built is
# refused at creation, and a child created without the restriction all the
# same is refused after the start. Any other modifier is a raw-command surface.
set(
    github_process_windows_launch_restriction
    "process->setCreateProcessArgumentsModifier([&inheritance, aFailStandardHandleList](QProcess::CreateProcessArguments* const aArguments) { if (aFailStandardHandleList) { aArguments->startupInfo->hStdError = nullptr; } inheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles); });"
)
string(
    REPLACE
    "${github_process_windows_launch_restriction}"
    ""
    github_process_runner_code_without_launch_restriction
    "${github_process_runner_code_compact}"
)
string(LENGTH "${github_process_runner_code_compact}" github_process_runner_code_length)
string(
    LENGTH
    "${github_process_runner_code_without_launch_restriction}"
    github_process_runner_code_without_launch_restriction_length
)
string(
    LENGTH
    "${github_process_windows_launch_restriction}"
    github_process_windows_launch_restriction_length
)
math(
    EXPR
    github_process_windows_launch_restriction_bytes
    "${github_process_runner_code_length} - ${github_process_runner_code_without_launch_restriction_length}"
)
if(
    NOT github_process_windows_launch_restriction_bytes
        EQUAL github_process_windows_launch_restriction_length
    OR NOT github_process_runner_code_compact
        MATCHES "#[ ]?include \"bpr/platform/windows_standard_handles\\.hpp\""
    OR NOT github_process_runner_code_compact
        MATCHES "bpr::platform::windows_standard_handles::launch_restriction inheritance;"
    OR NOT github_process_runner_code_compact
        MATCHES "if \\(!inheritance\\.restricted\\(\\)\\) \\{ return failed\\(github_cli_process_failure::start_failure\\); \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI process runner must restrict every Windows launch to its "
        "standard handles with exactly one platform launch restriction, and "
        "refuse a child started without it"
    )
endif()
if(
    github_process_runner_code_without_launch_restriction
        MATCHES "setCreateProcessArgumentsModifier"
)
    message(
        FATAL_ERROR
        "Forbidden shell/raw-command surface in "
        "${github_process_runner_relative}: a process-creation modifier other "
        "than the platform launch restriction"
    )
endif()
# [[BPR-0807]] The Git-acquisition factory is the only source of file limits,
# so its refusals must be code in its own body: a spool outside the store or
# over an existing file, a zero or oversized per-file limit, a relative store,
# and missing store thresholds. The body is matched as one span over code with
# comments removed, so a guard in a comment, a dead preprocessor branch or an
# uncalled function does not satisfy it. Exactly one definition may exist.
string(
    REGEX MATCHALL
    "make_github_cli_git_acquisition_process_runner"
    github_process_runner_acquisition_factories
    "${github_process_runner_code_compact}"
)
list(
    LENGTH
    github_process_runner_acquisition_factories
    github_process_runner_acquisition_factory_count
)
if(
    NOT github_process_runner_acquisition_factory_count EQUAL 1
    OR NOT github_process_runner_code_compact
        MATCHES
            "auto make_github_cli_git_acquisition_process_runner\\( ?github_cli_process_file_limits aFileLimits\\) -> std::unique_ptr<github_cli_process_port> \\{ if \\(!aFileLimits\\.StandardOutputFile\\.empty\\(\\) && \\(aFileLimits\\.StandardOutputFile\\.parent_path\\(\\) != aFileLimits\\.Directory \\|\\| std::filesystem::exists\\(aFileLimits\\.StandardOutputFile\\)\\)\\) \\{ return nullptr; \\} if \\(aFileLimits\\.MaximumFileBytes == 0U \\|\\| aFileLimits\\.MaximumFileBytes > 32U \\* 1024U \\* 1024U \\|\\| !aFileLimits\\.Directory\\.is_absolute\\(\\) \\|\\| aFileLimits\\.MaximumDirectoryBytes == 0U \\|\\| aFileLimits\\.MaximumDirectoryEntries == 0U \\|\\| aFileLimits\\.MaximumDirectoryEntries > 256U\\) \\{ return nullptr; \\} return make_qprocess_github_cli_process_runner\\( ?nullptr, std::move\\(aFileLimits\\)\\); \\}"
)
    message(
        FATAL_ERROR
        "GitHub CLI Git-acquisition factory must refuse, in its own body, a "
        "spool outside its store or over an existing file, a zero or oversized "
        "per-file limit, a relative store, and missing store thresholds"
    )
endif()

# [[BPR-0807]] RFC-BPR-0015's local pack construction spools one child's
# standard output into a new file inside the store the Git-acquisition factory
# admitted. Admit that one guarded call only when the code that follows it
# gives the same Unix child RLIMIT_FSIZE at the store's per-file limit. The
# span is matched over code with comments removed, so a limit in a comment,
# a dead preprocessor branch or an uncalled function does not admit it. Every
# other redirect is still refused. [[BPR-1252]]: the same child modifier then
# bounds the child's descriptor sweep, so it is installed for every launch and
# sets the limit whenever there is one.
set(
    github_process_runner_spool_pattern
    "if[ \t\r\n]*\\([ \t\r\n]*!aFileLimits\\.StandardOutputFile\\.empty\\(\\)[ \t\r\n]*\\)[ \t\r\n]*\\{[ \t\r\n]*process->setStandardOutputFile\\([ \t\r\n]*QString::fromStdString\\(aFileLimits\\.StandardOutputFile\\.string\\(\\)\\),[ \t\r\n]*QIODevice::Truncate\\);[ \t\r\n]*\\}"
)
set(
    github_process_runner_spool_code
    "if \\(!aFileLimits\\.StandardOutputFile\\.empty\\(\\)\\) \\{ process->setStandardOutputFile\\(QString::fromStdString\\(aFileLimits\\.StandardOutputFile\\.string\\(\\)\\), QIODevice::Truncate\\); \\}"
)
# Count the call token: the guarded text holds `;`, which a CMake list splits.
string(
    REGEX MATCHALL
    "setStandardOutputFile"
    github_process_runner_output_files
    "${github_process_runner_code_compact}"
)
list(
    LENGTH
    github_process_runner_output_files
    github_process_runner_output_file_count
)
set(
    github_process_runner_contents_with_spool
    "${github_process_runner_contents}"
)
if(github_process_runner_code_compact MATCHES "${github_process_runner_spool_code}")
    if(
        NOT github_process_runner_output_file_count EQUAL 1
        OR NOT github_process_runner_code_compact
            MATCHES
                "${github_process_runner_spool_code} #if defined\\(Q_OS_UNIX\\) process->setChildProcessModifier\\(\\[limit = aFileLimits\\.MaximumFileBytes, child = process\\.get\\(\\)\\] \\{ if \\(limit != 0U\\) \\{ const rlimit fileLimit\\{ static_cast<rlim_t>\\(limit\\), static_cast<rlim_t>\\(limit\\) \\}; if \\(::setrlimit\\(RLIMIT_FSIZE, &fileLimit\\) != 0\\) \\{ child->failChildProcessModifier\\(\"file size limit\", errno\\); \\} \\} bpr::platform::unix_descriptor_sweep::bound\\(\\); \\}\\);"
    )
        message(
            FATAL_ERROR
            "GitHub CLI process runner may spool standard output only once, "
            "into a new file inside its admitted store, and only where the same "
            "child sets RLIMIT_FSIZE"
        )
    endif()
    string(
        REGEX REPLACE
        "${github_process_runner_spool_pattern}"
        ""
        github_process_runner_contents
        "${github_process_runner_contents}"
    )
endif()
reject_github_process_runner_surface(
    "(QProcess::(ForwardedChannels|ForwardedOutputChannel|ForwardedErrorChannel|MergedChannels)|setProcessChannelMode[ \t\r\n]*\\([ \t\r\n]*QProcess::(Forwarded|Merged)|setStandardOutputProcess|setStandard(Input|Output|Error)File)"
    "forwarded, merged, or file-redirected process channel"
)
set(
    github_process_runner_contents
    "${github_process_runner_contents_with_spool}"
)

set(
    github_process_runner_without_selected_channels
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "(process|aProcess)(\\.|->)[ \t\r\n]*setReadChannel[ \t\r\n]*\\([ \t\r\n]*channel[ \t\r\n]*\\)"
    ""
    github_process_runner_without_selected_channels
    "${github_process_runner_without_selected_channels}"
)
string(
    REGEX REPLACE
    "process(\\.|->)[ \t\r\n]*setReadChannel[ \t\r\n]*\\([ \t\r\n]*QProcess::StandardOutput[ \t\r\n]*\\)"
    ""
    github_process_runner_without_selected_channels
    "${github_process_runner_without_selected_channels}"
)
if(
    github_process_runner_without_selected_channels
        MATCHES "setReadChannel[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Forbidden unselected process channel selection surface in "
        "${github_process_runner_relative}"
    )
endif()

set(
    github_process_runner_without_bounded_read
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "(process|aProcess)\\.read[ \t\r\n]*\\([ \t\r\n]*buffer\\.data[ \t\r\n]*\\([ \t]*\\)[ \t\r\n]*,[ \t\r\n]*static_cast<qint64>[ \t\r\n]*\\([ \t\r\n]*requested[ \t\r\n]*\\)[ \t\r\n]*\\)"
    ""
    github_process_runner_without_bounded_read
    "${github_process_runner_without_bounded_read}"
)
if(
    github_process_runner_without_bounded_read
        MATCHES "\\.[ \t\r\n]*read(All|Line)?[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Forbidden unselected raw process channel read surface in "
        "${github_process_runner_relative}"
    )
endif()
reject_github_process_runner_surface(
    "(\\.detach[ \t\r\n]*\\(|Worker\\.release[ \t\r\n]*\\(|new[ \t\r\n]+process_worker|static[ \t\r\n]+([^;\r\n]*[ \t\r\n]+)?process_worker|(^|[\r\n])[ \t]*process_worker[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[;={])"
    "global, detached, or leaked process worker"
)
reject_github_process_runner_surface(
    "(process\\.reset[ \t\r\n]*\\(|delete[ \t\r\n]+process|AwaitingReap\\.release[ \t\r\n]*\\(|new[ \t\r\n]+QProcess|(^|[;{}\r\n])[ \t]*QProcess[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*[;={])"
    "premature process ownership release"
)

set(
    github_process_runner_without_local_worker_view
    "${github_process_runner_contents}"
)
string(
    REGEX REPLACE
    "process_worker[ \t]*\\*[ \t]*worker[ \t]*=[ \t]*nullptr"
    ""
    github_process_runner_without_local_worker_view
    "${github_process_runner_without_local_worker_view}"
)
if(
    github_process_runner_without_local_worker_view
        MATCHES
            "(std::shared_ptr<process_worker>|process_worker[ \t]*\\*)"
)
    message(
        FATAL_ERROR
        "Forbidden unowned or shared process worker lifetime in "
        "${github_process_runner_relative}"
    )
endif()
reject_github_process_runner_surface(
    "(QProcessEnvironment::systemEnvironment|QProcessEnvironment::InheritFromParent|processEnvironment[ \t\r\n]*\\(|qgetenv[ \t\r\n]*\\(|qEnvironmentVariable[A-Za-z0-9_]*[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(getenv|secure_getenv|_wgetenv|environ)([^A-Za-z0-9_]|$))"
    "inherited or ambient environment"
)
reject_github_process_runner_surface(
    "errorString[ \t\r\n]*\\("
    "raw process error-string exposure"
)
# RFC-BPR-0078: only this closed constant table names child token variables.
# Generic requests still refuse them; the dedicated runner input owns the
# credential separately through worker/reap lifetime. Everything outside the
# exact table remains subject to the original credential-literal prohibition.
set(github_runner_credential_view "${github_process_runner_compact}")
set(github_runner_credential_table "constexpr std::array<std::string_view, 4U> credential_environment_names{ \"GH_TOKEN\", \"GITHUB_TOKEN\", \"GH_ENTERPRISE_TOKEN\", \"GITHUB_ENTERPRISE_TOKEN\" };")
string(REPLACE "${github_runner_credential_table}" "" github_runner_credential_view "${github_runner_credential_view}")
if(github_runner_credential_view MATCHES "(\"--(auth-token|with-token|token|include|cache|verbose|debug|jq|template|paginate|slurp|preview|field|raw-field|silent)\"|(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN|GH_DEBUG)([^A-Za-z0-9_]|$))")
    message(FATAL_ERROR "Forbidden credential, status-output, cache, or debug surface in ${github_process_runner_relative}")
endif()
if(github_process_runner_compact MATCHES "credential_environment_names")
    foreach(required_sensitive_fragment IN ITEMS
        "std::ranges::any_of(credential_environment_names,"
        "equal_environment_names(entry.Name, aName)"
        "item->Capture = std::move(aCapture)"
        "item->Credential = std::move(aCredential)"
        "aCapture.get()"
        "aCredential->view()"
        "outputFilter->finish"
        "capture->finish_credential()")
        string(FIND "${github_process_runner_compact}" "${required_sensitive_fragment}" sensitive_fragment_at)
        if(sensitive_fragment_at EQUAL -1)
            message(FATAL_ERROR "GitHub CLI sensitive ownership contract missing: ${required_sensitive_fragment}")
        endif()
    endforeach()
endif()
reject_github_process_runner_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(READ "${github_cli_transport_file}" github_cli_transport_contents)
set(
    required_github_cli_transport_patterns
    "\"api\""
    "\"--hostname\""
    "\"--include\""
    "\"--method\""
    "\"--input\""
    "\"\\.git\""
    "GH_PROMPT_DISABLED=1"
    "GH_NO_UPDATE_NOTIFIER=1"
    "GH_NO_EXTENSION_UPDATE_NOTIFIER=1"
    "GH_SPINNER_DISABLED=1"
    "NO_COLOR=1"
    "CLICOLOR=0"
    "\"HOME\""
    "\"PATH\""
    "\"XDG_RUNTIME_DIR\""
    "\"DBUS_SESSION_BUS_ADDRESS\""
    "\"USERPROFILE\""
    "\"APPDATA\""
    "\"LOCALAPPDATA\""
    "\"SystemRoot\""
    "\"PATH\""
    "\"HTTPS_PROXY\""
    "\"NO_PROXY\""
    "\"SSL_CERT_FILE\""
    "\"SSL_CERT_DIR\""
)
foreach(
    required_transport_pattern
    IN LISTS required_github_cli_transport_patterns
)
    if(NOT github_cli_transport_contents MATCHES "${required_transport_pattern}")
        message(
            FATAL_ERROR
            "GitHub CLI transport must retain explicit allowlisted "
            "invocation and minimal noninteractive environment construction: "
            "${required_transport_pattern}"
        )
    endif()
endforeach()

function(reject_github_cli_transport_surface forbidden_pattern surface_name)
    if(github_cli_transport_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${github_cli_transport_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_github_cli_transport_surface(
    "(#[ \t]*include[ \t]*[<\"]Q|(^|[^A-Za-z0-9_])Q(Process|ProcessEnvironment|String|ByteArray|Variant|Url)([^A-Za-z0-9_]|$))"
    "Qt/process implementation"
)
reject_github_cli_transport_surface(
    "\"--(auth-token|with-token|token|cache|verbose|debug|jq|template|paginate|slurp|preview|field|raw-field|silent)\""
    "credential, cache, debug, or response-transform flag"
)
reject_github_cli_transport_surface(
    "(^|[^A-Za-z0-9_])(Authorization|GH_TOKEN|GITHUB_TOKEN|GH_ENTERPRISE_TOKEN|GITHUB_ENTERPRISE_TOKEN|GH_DEBUG)([^A-Za-z0-9_]|$)"
    "credential or debug literal"
)
reject_github_cli_transport_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

get_filename_component(
    github_native_https_interface_file
    "${BPR_ROOT}/${github_native_https_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_native_https_file
    "${BPR_ROOT}/${github_native_https_relative}"
    ABSOLUTE
)
get_filename_component(
    github_wire_validation_interface_file
    "${BPR_ROOT}/${github_wire_validation_interface_relative}"
    ABSOLUTE
)
get_filename_component(
    github_wire_validation_file
    "${BPR_ROOT}/${github_wire_validation_relative}"
    ABSOLUTE
)

foreach(
    standard_native_header
    IN ITEMS
        "${github_native_https_interface_file}"
        "${github_wire_validation_interface_file}"
)
    file(READ "${standard_native_header}" standard_native_header_contents)
    set(
        forbidden_standard_native_header_patterns
        "#[ \t]*include[ \t]*[<\"]Q"
        "#[ \t]*include[ \t]*[<\"]Qt"
        "(^|[^A-Za-z0-9_])Q[A-Z][A-Za-z0-9_]*"
        "#[ \t]*include[ \t]*[<\"](windows\\.h|winhttp\\.h|Security/|SystemConfiguration/|sys/socket\\.h|curl/)"
        "(^|[^A-Za-z0-9_])(HANDLE|SOCKET|CF[A-Z][A-Za-z0-9_]*|CURL[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    )
    foreach(
        forbidden_standard_native_header_pattern
        IN LISTS forbidden_standard_native_header_patterns
    )
        if(
            standard_native_header_contents
                MATCHES "${forbidden_standard_native_header_pattern}"
        )
            file(
                RELATIVE_PATH
                relative_file
                "${BPR_ROOT}"
                "${standard_native_header}"
            )
            message(
                FATAL_ERROR
                "GitHub native HTTPS interfaces must remain standard C++ "
                "and effect-free in ${relative_file}: "
                "${forbidden_standard_native_header_pattern}"
            )
        endif()
    endforeach()
endforeach()

file(READ "${github_native_https_file}" github_native_https_contents)
file(READ "${github_wire_validation_file}" github_wire_validation_contents)
file(READ "${github_cli_transport_file}" github_cli_transport_contents)

set(
    required_native_https_patterns
    "#[ \t]*include[ \t]*[<\"]QNetworkAccessManager[>\"]"
    "#[ \t]*include[ \t]*[<\"]QSslConfiguration[>\"]"
    "QNetworkProxyFactory::systemProxyForQuery[ \t\r\n]*\\("
    "maximum_system_proxy_candidates"
    "proxy_candidate_retryable[ \t\r\n]*\\("
    "QNetworkRequest::ManualRedirectPolicy"
    "QSslSocket::VerifyPeer"
    "QSsl::TlsV1_2OrLater"
    "QSslConfiguration::systemCaCertificates[ \t\r\n]*\\("
    "setLocalCertificateChain[ \t\r\n]*\\([ \t\r\n]*\\{[ \t\r\n]*\\}[ \t\r\n]*\\)"
    "setBackendConfiguration[ \t\r\n]*\\([ \t\r\n]*\\{[ \t\r\n]*\\}[ \t\r\n]*\\)"
    "QNetworkAccessManager::sslErrors"
    "QNetworkReply::encrypted"
    "QNetworkRequest::Http2AllowedAttribute"
    "\"Accept-Encoding\""
    "\"identity\""
    "\"Accept-Language\""
    "std::stop_callback"
    "active_hop_guard"
    "remaining_before_deadline[ \t\r\n]*\\("
    "Done\\.notify_one[ \t\r\n]*\\("
    "setTransferTimeout[ \t\r\n]*\\("
    "setReadBufferSize[ \t\r\n]*\\("
    "validate_github_redirect_target[ \t\r\n]*\\("
    "validate_github_wire_request[ \t\r\n]*\\("
    "BPR_NATIVE_HTTPS_TESTING"
    "QSslSocket::activeBackend[ \t\r\n]*\\("
)
foreach(required_native_https_pattern IN LISTS required_native_https_patterns)
    if(
        NOT github_native_https_contents
            MATCHES "${required_native_https_pattern}"
    )
        message(
            FATAL_ERROR
            "GitHub native HTTPS must retain explicit system proxy/trust, "
            "manual redirect, cancellation, protocol, and resource policy: "
            "${required_native_https_pattern}"
        )
    endif()
endforeach()

foreach(
    validation_consumer_contents
    IN ITEMS
        "${github_native_https_contents}"
        "${github_cli_transport_contents}"
)
    if(
        NOT validation_consumer_contents
            MATCHES
                "#[ \t]*include[ \t]*[<\"]github_wire_request_validation\\.hpp[>\"]"
    )
        message(
            FATAL_ERROR
            "GitHub transports must use the shared correlation and outbound "
            "request validator"
        )
    endif()
endforeach()
if(
    NOT github_wire_validation_contents
        MATCHES "validate_outbound_read_request[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Shared GitHub wire validation must recompile and validate the "
        "canonical read-only request"
    )
endif()

function(reject_native_https_surface forbidden_pattern surface_name)
    if(github_native_https_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden native HTTPS ${surface_name} surface in "
            "${github_native_https_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_native_https_surface(
    "(ignoreSslErrors|QSslSocket::VerifyNone|setPeerVerifyName[ \t\r\n]*\\()"
    "certificate bypass"
)
reject_native_https_surface(
    "(QSslConfiguration::setDefaultConfiguration|QSslSocket::setActiveBackend)"
    "process-global TLS mutation"
)
reject_native_https_surface(
    "(QNetworkProxy::setApplicationProxy|QNetworkProxyFactory::setUseSystemConfiguration|QNetworkProxyFactory::setApplicationProxyFactory)"
    "global proxy mutation"
)
reject_native_https_surface(
    "(QNetworkRequest::(NoLessSafeRedirectPolicy|SameOriginRedirectPolicy|UserVerifiedRedirectPolicy)|FollowRedirectsAttribute)"
    "automatic redirect"
)
reject_native_https_surface(
    "\\.[ \t\r\n]*read(All|Line)[ \t\r\n]*\\("
    "unbounded response read"
)
reject_native_https_surface(
    "(errorString|reasonPhrase|std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "raw diagnostic or logging"
)
reject_native_https_surface(
    "(QNetworkDiskCache|setCookieJar[ \t\r\n]*\\(|QSettings|QFile|QProcess|QTcp(Server|Socket)|QUdpSocket|QLocalServer)"
    "persistence, process, listener, or raw socket"
)
reject_native_https_surface(
    "(qgetenv[ \t\r\n]*\\(|qEnvironmentVariable[A-Za-z0-9_]*[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(getenv|secure_getenv|_wgetenv|environ)([^A-Za-z0-9_]|$))"
    "ambient environment"
)

file(READ "${macos_keychain_file}" macos_keychain_contents)
if(
    NOT macos_keychain_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
    )
    message(
        FATAL_ERROR
        "macOS Keychain adapter requires the host effect executor"
    )
endif()

set(
    required_macos_keychain_patterns
    "#[ \t]*include[ \t]*[<\"]Security/(Security|SecItem|SecKeychain)\\.h[>\"]"
    "SecKeychainCopyDefault[ \t\r\n]*\\("
    "SecItemAdd[ \t\r\n]*\\("
    "SecItemCopyMatching[ \t\r\n]*\\("
    "SecItemUpdate[ \t\r\n]*\\("
    "SecItemDelete[ \t\r\n]*\\("
)
foreach(required_pattern IN LISTS required_macos_keychain_patterns)
    if(NOT macos_keychain_contents MATCHES "${required_pattern}")
        message(
            FATAL_ERROR
            "macOS Keychain adapter must use the selected Keychain lifecycle: "
            "${required_pattern}"
        )
    endif()
endforeach()

set(
    required_macos_keychain_query_constants
    kSecClassGenericPassword
    kSecAttrService
    kSecAttrAccount
    kSecUseKeychain
    kSecMatchSearchList
    kSecUseAuthenticationUI
    kSecUseAuthenticationUIFail
    kSecReturnData
    kSecMatchLimit
    kSecMatchLimitOne
)
foreach(
    required_query_constant
    IN LISTS required_macos_keychain_query_constants
)
    if(
        NOT macos_keychain_contents
            MATCHES
                "(^|[^A-Za-z0-9_])${required_query_constant}([^A-Za-z0-9_]|$)"
    )
        message(
            FATAL_ERROR
            "macOS Keychain adapter must retain the exact-keychain, "
            "noninteractive, data-only query property "
            "${required_query_constant}"
        )
    endif()
endforeach()

set(macos_keychain_effects "${macos_keychain_contents}")
foreach(
    selected_effect
    IN ITEMS
        SecKeychainCopyDefault
        SecKeychainRef
        SecItemAdd
        SecItemCopyMatching
        SecItemUpdate
        SecItemDelete
)
    string(REPLACE "${selected_effect}" "" macos_keychain_effects "${macos_keychain_effects}")
endforeach()
if(
    macos_keychain_effects
        MATCHES
            "(^|[^A-Za-z0-9_])Sec(Item|Keychain)[A-Za-z0-9_]+"
)
    message(
        FATAL_ERROR
        "macOS Keychain adapter uses a Keychain effect outside the selected "
        "SecKeychainCopyDefault and SecItem lifecycle"
    )
endif()

function(reject_macos_keychain_surface forbidden_pattern surface_name)
    if(macos_keychain_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in ${macos_keychain_relative}: "
            "${forbidden_pattern}"
        )
    endif()
endfunction()

reject_macos_keychain_surface(
    "kSec(UseDataProtectionKeychain|AttrSynchronizable|AttrAccessGroup|AttrAccessible[A-Za-z0-9_]*)"
    "Keychain backend-switch"
)
reject_macos_keychain_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_macos_keychain_surface(
    "kSec(Class(InternetPassword|Certificate|Key|Identity)|Return(Attributes|Ref|PersistentRef)|MatchLimitAll|UseAuthenticationUI(Allow|Skip))"
    "Keychain query-policy"
)
reject_macos_keychain_surface(
    "(SecCopyErrorMessageString|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string)|([io]stringstream|snprintf|sprintf)[ \t\r\n]*\\(|QString::number)"
    "raw operating-system error formatting"
)
reject_macos_keychain_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|AppKit/|UIKit/)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|NS(Application|Window|View|Alert)[A-Za-z0-9_]*|UI(Application|Window|View|Alert)[A-Za-z0-9_]*))"
    "Qt/UI"
)
reject_macos_keychain_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|CFNetwork|NSURLSession|NWConnection|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*)"
    "network"
)
reject_macos_keychain_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|NSUserDefaults|CFPreferences)"
    "settings/database"
)
reject_macos_keychain_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite)[ \t\r\n]*\\()"
    "filesystem"
)
reject_macos_keychain_surface(
    "((^|[^A-Za-z0-9_])(getenv|setenv|unsetenv|putenv)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment|NSProcessInfo)"
    "environment"
)
reject_macos_keychain_surface(
    "(QProcess|NSTask|posix_spawn|(^|[^A-Za-z0-9_])(system|popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_macos_keychain_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|os_log|NSLog|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(
    READ
    "${windows_credential_manager_file}"
    windows_credential_manager_contents
)
if(
    NOT windows_credential_manager_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
)
    message(
        FATAL_ERROR
        "Windows Credential Manager adapter requires the host effect executor"
    )
endif()

set(
    required_windows_credential_manager_patterns
    "#[ \t]*include[ \t]*[<\"]wincred\\.h[>\"]"
    "CredWriteW[ \t\r\n]*\\("
    "CredReadW[ \t\r\n]*\\("
    "CredDeleteW[ \t\r\n]*\\("
    "CredFree[ \t\r\n]*\\("
    "GetLastError[ \t\r\n]*\\("
    "SecureZeroMemory[ \t\r\n]*\\("
)
foreach(
    required_pattern
    IN LISTS required_windows_credential_manager_patterns
)
    if(NOT windows_credential_manager_contents MATCHES "${required_pattern}")
        message(
            FATAL_ERROR
            "Windows Credential Manager adapter must use the selected "
            "wide-character lifecycle: ${required_pattern}"
        )
    endif()
endforeach()

set(
    required_windows_credential_manager_constants
    CRED_TYPE_GENERIC
    CRED_PERSIST_LOCAL_MACHINE
    CRED_MAX_GENERIC_TARGET_NAME_LENGTH
    windows_credential_manager_maximum_blob_bytes
)
foreach(
    required_constant
    IN LISTS required_windows_credential_manager_constants
)
    if(
        NOT windows_credential_manager_contents
            MATCHES
                "(^|[^A-Za-z0-9_])${required_constant}([^A-Za-z0-9_]|$)"
    )
        message(
            FATAL_ERROR
            "Windows Credential Manager adapter must retain the selected "
            "generic, local-machine, bounded credential property "
            "${required_constant}"
        )
    endif()
endforeach()

set(
    required_windows_credential_manager_bounds
    "<=[ \t\r\n]*CRED_MAX_GENERIC_TARGET_NAME_LENGTH"
    "windows_credential_manager_maximum_blob_bytes[ \t\r\n]*=[ \t\r\n]*5U[ \t\r\n]*\\*[ \t\r\n]*512U"
    "application::maximum_github_credential_bytes[ \t\r\n]*==[ \t\r\n]*windows_credential_manager_maximum_blob_bytes"
    "CredentialBlobSize[ \t\r\n]*<=[ \t\r\n]*windows_credential_manager_maximum_blob_bytes"
)
foreach(required_bound IN LISTS required_windows_credential_manager_bounds)
    if(NOT windows_credential_manager_contents MATCHES "${required_bound}")
        message(
            FATAL_ERROR
            "Windows credential bound missing: ${required_bound}"
        )
    endif()
endforeach()

function(
    reject_windows_credential_manager_surface
    forbidden_pattern
    surface_name
)
    if(windows_credential_manager_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${windows_credential_manager_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_windows_credential_manager_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_windows_credential_manager_surface(
    "CRED_MAX_CREDENTIAL_BLOB_SIZE"
    "toolchain-varying legacy credential-blob bound"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])(Cred(Write|Read|Delete)A?|P?CREDENTIALA?)([^A-Za-z0-9_]|$)"
    "ANSI/encoding-neutral Credential Manager"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])Cred(UI|Enumerate|FindBestCredential|GetTargetInfo|ReadDomainCredentials|WriteDomainCredentials|IsProtected|Protect|Unprotect|Rename|MarshalCredential|UnmarshalCredential)[A-Za-z0-9_]*"
    "unselected Credential Manager"
)

set(
    windows_credential_manager_effects
    "${windows_credential_manager_contents}"
)
foreach(
    selected_effect
    IN ITEMS
        CredWriteW
        CredReadW
        CredDeleteW
        CredFree
)
    string(
        REPLACE
        "${selected_effect}"
        ""
        windows_credential_manager_effects
        "${windows_credential_manager_effects}"
    )
endforeach()
if(
    windows_credential_manager_effects
        MATCHES
            "(^|[^A-Za-z0-9_])Cred[A-Z][A-Za-z0-9_]*"
)
    message(
        FATAL_ERROR
        "Windows Credential Manager adapter uses a credential effect outside "
        "the selected CredWriteW, CredReadW, CredDeleteW, and CredFree "
        "lifecycle"
    )
endif()

reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"](dpapi|wincrypt|ncryptprotect)\\.h[>\"]|(^|[^A-Za-z0-9_])(CryptProtectData|CryptUnprotectData|CryptProtectMemory|CryptUnprotectMemory|NCryptProtectSecret|NCryptUnprotectSecret|DATA_BLOB|NCRYPT_DESCRIPTOR_HANDLE)([^A-Za-z0-9_]|$)|Crypt32)"
    "DPAPI"
)
reject_windows_credential_manager_surface(
    "(Windows[.:]+Security[.:]+Credentials|PasswordVault|PasswordCredential|#[ \t]*include[ \t]*[<\"]winrt/Windows\\.Security\\.Credentials\\.h[>\"])"
    "Credential Locker"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_PERSIST_(SESSION|ENTERPRISE)([^A-Za-z0-9_]|$)"
    "non-local Credential Manager persistence"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_PRESERVE_CREDENTIAL_BLOB([^A-Za-z0-9_]|$)"
    "credential-blob preservation"
)
reject_windows_credential_manager_surface(
    "(^|[^A-Za-z0-9_])CRED_TYPE_(DOMAIN_PASSWORD|DOMAIN_CERTIFICATE|DOMAIN_VISIBLE_PASSWORD|GENERIC_CERTIFICATE|DOMAIN_EXTENDED|MAXIMUM|MAXIMUM_EX)([^A-Za-z0-9_]|$)"
    "non-generic credential type"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"]winreg\\.h[>\"]|(^|[^A-Za-z0-9_])(Reg(Open|Create|Set|Get|Query|Delete|Close|Load|Save|Restore|Replace|Copy|Enum|Flush|Connect)[A-Za-z0-9_]*|HKEY_[A-Z0-9_]+)([^A-Za-z0-9_]|$))"
    "registry"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite|((Create|Open|Read|Write|Delete|Move|Copy|Replace)File)[AW]?)[ \t\r\n]*\\()"
    "filesystem"
)
reject_windows_credential_manager_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|ApplicationData[.:]+Current|LocalSettings|RoamingSettings)"
    "settings/database"
)
reject_windows_credential_manager_surface(
    "((^|[^A-Za-z0-9_])(_?w?getenv|_?w?putenv|setenv|unsetenv|GetEnvironment(Strings|Variable)[AW]?|FreeEnvironmentStrings[AW]?|SetEnvironmentVariable[AW]?)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment)"
    "environment"
)
reject_windows_credential_manager_surface(
    "(QProcess|CreateProcess(AsUser|WithLogon|WithToken)?[AW]?|ShellExecute(Ex)?[AW]?|WinExec|(^|[^A-Za-z0-9_])(system|popen|_popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_windows_credential_manager_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|WinHttp[A-Za-z0-9_]*|Internet(Open|Connect|ReadFile|WriteFile|CloseHandle)[AW]?|URLDownloadToFile[AW]?|Windows[.:]+Web[.:]+Http|HttpClient|IXMLHTTPRequest|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(socket|connect|send|recv|WSA[A-Za-z0-9_]*)[ \t\r\n]*\\()"
    "network"
)
reject_windows_credential_manager_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|Windows\\.UI)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|MessageBox[AW]?|CreateWindow(Ex)?[AW]?|DialogBox[A-Za-z0-9_]*|TaskDialog[A-Za-z0-9_]*)([^A-Za-z0-9_]|$))"
    "Qt/UI"
)
reject_windows_credential_manager_surface(
    "(FormatMessage[AW]?|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string|to_wstring)|([io]stringstream|snprintf|sprintf|swprintf)[ \t\r\n]*\\(|QString::number|std::system_category|std::error_code)"
    "raw operating-system error formatting"
)
reject_windows_credential_manager_surface(
    "(std::(cout|cerr|clog|wcout|wcerr|wclog)|TraceLogging[A-Za-z0-9_]*|EventWrite[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(printf|fprintf|wprintf|fwprintf|puts|fputs|perror|syslog|OutputDebugString[AW]?|ReportEvent[AW]?|qDebug|qInfo|qWarning|qCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)

file(
    READ
    "${linux_secret_service_file}"
    linux_secret_service_contents
)
string(
    REGEX REPLACE
    "\"[ \t\r\n]*\""
    ""
    linux_secret_service_normalized_contents
    "${linux_secret_service_contents}"
)

if(
    NOT linux_secret_service_contents
        MATCHES
            "(^|[^A-Za-z0-9_])application::effect_executor([^A-Za-z0-9_]|$)"
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter requires the host effect executor"
    )
endif()

set(
    required_linux_secret_service_patterns
    "#[ \t]*include[ \t]*[<\"]libsecret/secret\\.h[>\"]"
    "canonical_service_bus_name[ \t\r\n]*=[ \t\r\n]*\"org\\.freedesktop\\.secrets\""
    "G_DEFINE_TYPE[ \t\r\n]*\\([^\\)]*non_prompting_secret_service[^\\)]*SECRET_TYPE_SERVICE[ \t\r\n]*\\)"
    "secret_service_open_sync[ \t\r\n]*\\([ \t\r\n]*non_prompting_secret_service_get_type[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*c_str[ \t\r\n]*\\([ \t\r\n]*canonical_service_bus_name[ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*SECRET_SERVICE_NONE"
    "if[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*\\)[ \t\r\n]*\\{[ \t\r\n]*const[ \t\r\n]+auto[ \t\r\n]+failure[ \t\r\n]*=[ \t\r\n]*mapped_error[ \t\r\n]*\\([ \t\r\n]*error\\.get[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*;"
    "failure[ \t\r\n]*==[ \t\r\n]*application::github_credential_vault_failure::[ \t\r\n]*operation_failed[ \t\r\n]*\\?[ \t\r\n]*application::github_credential_vault_failure::[ \t\r\n]*unavailable[ \t\r\n]*:[ \t\r\n]*failure"
    "g_dbus_proxy_get_name[ \t\r\n]*\\([ \t\r\n]*G_DBUS_PROXY[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*\\)"
    "if[ \t\r\n]*\\([ \t\r\n]*actual_name[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*actual_name[ \t\r\n]*,[ \t\r\n]*c_str[ \t\r\n]*\\([ \t\r\n]*canonical_service_bus_name[ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*!=[ \t\r\n]*0[ \t\r\n]*\\)"
    "g_object_unref[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*\\)[ \t\r\n]*;[ \t\r\n]*Service[ \t\r\n]*=[ \t\r\n]*nullptr[ \t\r\n]*;[ \t\r\n]*return[ \t\r\n]+std::unexpected[ \t\r\n]*\\{[ \t\r\n]*application::github_credential_vault_failure::unavailable"
    "secret_service_ensure_session_sync[ \t\r\n]*\\([ \t\r\n]*Service[ \t\r\n]*,[ \t\r\n]*nullptr[ \t\r\n]*,[ \t\r\n]*session_error\\.out[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "mapped_error[ \t\r\n]*\\([ \t\r\n]*session_error\\.get[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "secret_collection_for_alias_sync[ \t\r\n]*\\([^\\)]*SECRET_COLLECTION_DEFAULT[ \t\r\n]*,[ \t\r\n]*SECRET_COLLECTION_NONE"
    "production_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.v2\""
    "integration_test_schema_identifier[ \t\r\n]*=[ \t\r\n]*\"org\\.betterpullreview\\.github-native-credential\\.secret-service\\.integration-test\\.v2\""
    "authority_attribute[ \t\r\n]*=[ \t\r\n]*\"authority\""
    "schema_attribute[ \t\r\n]*=[ \t\r\n]*\"xdg:schema\""
    "g_hash_table_insert[ \t\r\n]*\\([^;]*const_cast<char\\*>[ \t\r\n]*\\([ \t\r\n]*c_str[ \t\r\n]*\\([ \t\r\n]*authority_attribute[ \t\r\n]*\\)[ \t\r\n]*\\)"
    "g_hash_table_lookup[ \t\r\n]*\\([^;]*c_str[ \t\r\n]*\\([ \t\r\n]*authority_attribute[ \t\r\n]*\\)[ \t\r\n]*\\)"
    "g_hash_table_lookup[ \t\r\n]*\\([^;]*c_str[ \t\r\n]*\\([ \t\r\n]*schema_attribute[ \t\r\n]*\\)[ \t\r\n]*\\)"
    "SECRET_SCHEMA_NONE"
    "SECRET_SCHEMA_ATTRIBUTE_STRING"
    "g_hash_table_size[^\r\n]*!=[ \t]*2U"
    "static_assert[ \t\r\n]*\\([ \t\r\n]*linux_secret_service_authority_account_bytes[ \t\r\n]*==[ \t\r\n]*574U[ \t\r\n]*\\)"
    "nibble[ \t\r\n]*<[ \t\r\n]*16U"
    "authority\\.Profile\\.value[ \t\r\n]*\\("
    "authority\\.ProfileRevision\\.value[ \t\r\n]*\\("
    "authority\\.Host\\.value[ \t\r\n]*\\("
    "authority\\.Account\\.value[ \t\r\n]*\\("
    "append_hex_u8[ \t\r\n]*\\([^\\)]*server_contract_discriminator[ \t\r\n]*\\([ \t\r\n]*authority\\.ServerContract"
    "authority\\.Hostname\\.value[ \t\r\n]*\\("
    "static_cast<std::uint8_t>[ \t\r\n]*\\([ \t\r\n]*hostname\\.size[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\)"
    "index[ \t\r\n]*<[ \t\r\n]*application::maximum_github_hostname_bytes"
    "static_cast<unsigned[ \t\r\n]+char>[ \t\r\n]*\\([ \t\r\n]*hostname\\[index\\][ \t\r\n]*\\)"
    "append_hex_u8[ \t\r\n]*\\([ \t\r\n]*encoded[ \t\r\n]*,[ \t\r\n]*offset[ \t\r\n]*,[ \t\r\n]*value[ \t\r\n]*\\)"
    "binary_content_type[ \t\r\n]*=[ \t\r\n]*\"application/octet-stream\""
    "gnome_keyring_content_type[ \t\r\n]*=[ \t\r\n]*\"text/plain\""
    "secret_value_new[ \t\r\n]*\\("
    "secret_value_get_content_type[ \t\r\n]*\\("
    "recognized_content_type[ \t\r\n]*=[ \t\r\n]*content_type[ \t\r\n]*!=[ \t\r\n]*nullptr[ \t\r\n]*&&[ \t\r\n]*\\([ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*content_type[ \t\r\n]*,[ \t\r\n]*c_str[ \t\r\n]*\\([ \t\r\n]*binary_content_type[ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*==[ \t\r\n]*0[ \t\r\n]*[|][|][ \t\r\n]*std::strcmp[ \t\r\n]*\\([ \t\r\n]*content_type[ \t\r\n]*,[ \t\r\n]*c_str[ \t\r\n]*\\([ \t\r\n]*gnome_keyring_content_type[ \t\r\n]*\\)[ \t\r\n]*\\)[ \t\r\n]*==[ \t\r\n]*0[ \t\r\n]*\\)[ \t\r\n]*;"
    "if[ \t\r\n]*\\([ \t\r\n]*value[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*content_type[ \t\r\n]*==[ \t\r\n]*nullptr[ \t\r\n]*[|][|][ \t\r\n]*![ \t\r\n]*recognized_content_type[ \t\r\n]*\\)"
    "secret_value_get[ \t\r\n]*\\("
    "secret_value_unref[ \t\r\n]*\\("
    "application::maximum_github_credential_bytes"
    "secret_collection_search_sync[ \t\r\n]*\\("
    "SECRET_SEARCH_ALL"
    "secret_item_create_sync[ \t\r\n]*\\("
    "SECRET_ITEM_CREATE_REPLACE"
    "secret_item_load_secret_sync[ \t\r\n]*\\("
    "secret_item_get_secret[ \t\r\n]*\\("
    "secret_item_delete_sync[ \t\r\n]*\\("
    "secret_collection_get_locked[ \t\r\n]*\\("
    "secret_item_get_locked[ \t\r\n]*\\("
    "prompt_sync[ \t\r\n]*=[ \t\r\n]*deny_prompt_sync"
    "prompt_async[ \t\r\n]*=[ \t\r\n]*deny_prompt_async"
    "prompt_finish[ \t\r\n]*=[ \t\r\n]*deny_prompt_finish"
    "deny_prompt_sync[^{]*\\{[^}]*dismiss_prompt[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)"
    "deny_prompt_async[^{]*\\{[^}]*dismiss_prompt[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)"
    "prompt_dismiss_timeout_milliseconds[ \t\r\n]*=[ \t\r\n]*[1-9][0-9']*"
    "g_dbus_proxy_call_sync[ \t\r\n]*\\([ \t\r\n]*G_DBUS_PROXY[ \t\r\n]*\\([ \t\r\n]*prompt[ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*\"Dismiss\"[ \t\r\n]*,[ \t\r\n]*g_variant_new[ \t\r\n]*\\([ \t\r\n]*\"\\(\\)\"[ \t\r\n]*\\)[ \t\r\n]*,[ \t\r\n]*G_DBUS_CALL_FLAGS_NO_AUTO_START[ \t\r\n]*,[ \t\r\n]*prompt_dismiss_timeout_milliseconds"
    "SECRET_ERROR_IS_LOCKED"
    "consteval[ \t\r\n]+auto[ \t\r\n]+c_str[ \t\r\n]*\\([ \t\r\n]*const[ \t\r\n]+std::string_view[ \t\r\n]+text[ \t\r\n]*\\)[ \t\r\n]*->[ \t\r\n]*const[ \t\r\n]+char\\*"
    "text\\.data[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\[[ \t\r\n]*text\\.size[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*\\][ \t\r\n]*!="
    "throw[ \t\r\n]+\"a string_view handed to a libsecret entry point must be null-terminated\""
)
foreach(required_pattern IN LISTS required_linux_secret_service_patterns)
    if(
        NOT linux_secret_service_normalized_contents
            MATCHES "${required_pattern}"
    )
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must retain the selected exact, "
            "binary, noninteractive lifecycle: ${required_pattern}"
        )
    endif()
endforeach()

string(
    REGEX REPLACE
    "[ \t\r\n]+"
    " "
    linux_secret_service_compact_contents
    "${linux_secret_service_contents}"
)

string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_service_open_sync"
    linux_secret_service_open_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "c_str(canonical_service_bus_name)) != 0"
    linux_secret_service_name_validation_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_service_ensure_session_sync"
    linux_secret_service_session_offset
)
string(
    FIND
    "${linux_secret_service_compact_contents}"
    "secret_collection_for_alias_sync"
    linux_secret_service_collection_offset
)
if(
    linux_secret_service_open_offset LESS 0
    OR linux_secret_service_name_validation_offset LESS 0
    OR linux_secret_service_session_offset LESS 0
    OR linux_secret_service_collection_offset LESS 0
    OR NOT linux_secret_service_open_offset
        LESS linux_secret_service_name_validation_offset
    OR NOT linux_secret_service_name_validation_offset
        LESS linux_secret_service_session_offset
    OR NOT linux_secret_service_session_offset
        LESS linux_secret_service_collection_offset
)
    message(
        FATAL_ERROR
        "Linux Secret Service session ordering invalid: open without a "
        "session, validate the canonical name, then ensure the session"
    )
endif()

math(
    EXPR
    linux_secret_service_proxy_lifecycle_length
    "${linux_secret_service_collection_offset} - ${linux_secret_service_open_offset}"
)
string(
    SUBSTRING
    "${linux_secret_service_compact_contents}"
    "${linux_secret_service_open_offset}"
    "${linux_secret_service_proxy_lifecycle_length}"
    linux_secret_service_proxy_lifecycle
)
string(
    REGEX MATCHALL
    "failure[ ]*==[ ]*application::github_credential_vault_failure::[ ]*operation_failed[ ]*\\?[ ]*application::github_credential_vault_failure::[ ]*unavailable[ ]*:[ ]*failure"
    linux_secret_service_normalized_failures
    "${linux_secret_service_proxy_lifecycle}"
)
list(
    LENGTH
    linux_secret_service_normalized_failures
    linux_secret_service_normalized_failure_count
)
if(NOT linux_secret_service_normalized_failure_count EQUAL 2)
    message(
        FATAL_ERROR
        "Linux Secret Service failure normalization invalid: both open and "
        "ensure-session operation failures must become unavailable"
    )
endif()

string(
    REGEX MATCHALL
    "g_object_unref[ ]*\\([ ]*Service[ ]*\\)"
    linux_secret_service_proxy_cleanups
    "${linux_secret_service_proxy_lifecycle}"
)
list(
    LENGTH
    linux_secret_service_proxy_cleanups
    linux_secret_service_proxy_cleanup_count
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "secret_service_ensure_session_sync"
    linux_secret_service_lifecycle_session_offset
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "mapped_error(session_error.get())"
    linux_secret_service_session_error_offset
)
string(
    FIND
    "${linux_secret_service_proxy_lifecycle}"
    "g_object_unref(Service)"
    linux_secret_service_session_cleanup_offset
    REVERSE
)
if(
    NOT linux_secret_service_proxy_cleanup_count EQUAL 2
    OR linux_secret_service_session_error_offset LESS 0
    OR linux_secret_service_session_cleanup_offset LESS 0
    OR NOT linux_secret_service_lifecycle_session_offset
        LESS linux_secret_service_session_error_offset
    OR NOT linux_secret_service_session_error_offset
        LESS linux_secret_service_session_cleanup_offset
)
    message(
        FATAL_ERROR
        "Linux Secret Service ensure-session failure cleanup invalid"
    )
endif()

if(
    NOT linux_secret_service_compact_contents
        MATCHES
            "secret_value_new[ ]*\\([ ]*reinterpret_cast<const char\\*>[ ]*\\([ ]*bytes\\.data[ ]*\\([ ]*\\)[ ]*\\)[ ]*,[ ]*static_cast<gssize>[ ]*\\([ ]*bytes\\.size[ ]*\\([ ]*\\)[ ]*\\)[ ]*,[ ]*c_str[ ]*\\([ ]*binary_content_type[ ]*\\)[ ]*\\)"
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must store only application/octet-stream "
        "SecretValue bytes"
    )
endif()

foreach(
    content_type_identifier_and_count
    IN ITEMS
        "binary_content_type;3"
        "gnome_keyring_content_type;2"
)
    list(GET content_type_identifier_and_count 0 content_type_identifier)
    list(GET content_type_identifier_and_count 1 expected_content_type_count)
    string(
        REGEX MATCHALL
        "${content_type_identifier}"
        linux_content_type_uses
        "${linux_secret_service_contents}"
    )
    list(LENGTH linux_content_type_uses actual_content_type_count)
    if(NOT actual_content_type_count EQUAL expected_content_type_count)
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must recognize exactly the selected "
            "two content types without broadening storage or retrieval"
        )
    endif()
endforeach()

string(
    REGEX MATCHALL
    "secret_service_open_sync[ \t\r\n]*\\("
    linux_secret_service_open_calls
    "${linux_secret_service_contents}"
)
list(LENGTH linux_secret_service_open_calls linux_secret_service_open_count)
if(NOT linux_secret_service_open_count EQUAL 1)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must open exactly one independently "
        "pinned service proxy"
    )
endif()

string(
    REGEX MATCHALL
    "secret_service_ensure_session_sync[ \t\r\n]*\\("
    linux_secret_service_session_calls
    "${linux_secret_service_contents}"
)
list(
    LENGTH
    linux_secret_service_session_calls
    linux_secret_service_session_count
)
if(NOT linux_secret_service_session_count EQUAL 1)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must ensure exactly one session after "
        "canonical service-name validation"
    )
endif()

string(
    REGEX MATCHALL
    "const[ \t]+auto[ \t]+service[ \t]*=[ \t]*service_proxy"
    linux_verified_service_gates
    "${linux_secret_service_contents}"
)
list(LENGTH linux_verified_service_gates linux_verified_service_gate_count)
# [[BPR-1068]] The fourth gate is the store-readiness question a headless
# sign-in asks before it starts; it reaches no item.
if(NOT linux_verified_service_gate_count EQUAL 4)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter must verify the selected service before "
        "every store, retrieve, and delete item lifecycle and the store-readiness check"
    )
endif()

function(reject_linux_secret_service_surface forbidden_pattern surface_name)
    if(linux_secret_service_contents MATCHES "${forbidden_pattern}")
        message(
            FATAL_ERROR
            "Forbidden ${surface_name} surface in "
            "${linux_secret_service_relative}: ${forbidden_pattern}"
        )
    endif()
endfunction()

reject_linux_secret_service_surface(
    "(application::decision_executor|DecisionLane|decision_lane)"
    "serialized decision-executor"
)
reject_linux_secret_service_surface(
    "(SECRET_SERVICE_BUS_NAME|DBUS_SESSION_BUS_ADDRESS|XDG_RUNTIME_DIR|(^|[^A-Za-z0-9_])(g_)?(getenv|setenv|unsetenv|putenv|listenv)[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$)|QProcessEnvironment)"
    "environment/service-name redirection"
)
reject_linux_secret_service_surface(
    "SECRET_SERVICE_OPEN_SESSION"
    "session establishment during service open"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_service_(get|get_sync|store|store_sync|lookup|lookup_sync|clear|clear_sync|search|search_sync|unlock|unlock_sync|create_collection|create_collection_sync)([^A-Za-z0-9_]|$)|(^|[^A-Za-z0-9_])secret_password_[A-Za-z0-9_]*|SECRET_SEARCH_(UNLOCK|LOAD_SECRETS)|SECRET_COLLECTION_SESSION)"
    "global/convenience Secret Service"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_(collection|service)_(create|delete|unlock)[A-Za-z0-9_]*|\"CreateCollection\"|\"DeleteCollection\"|\"Unlock\")"
    "collection creation/deletion/unlock"
)
reject_linux_secret_service_surface(
    "((^|[^A-Za-z0-9_])secret_(service_prompt|prompt_perform|prompt_run)[A-Za-z0-9_]*|\"Prompt\"|org\\.freedesktop\\.Secret\\.Prompt\\.Prompt)"
    "interactive prompt"
)
reject_linux_secret_service_surface(
    "(org\\.freedesktop\\.Secret\\.Service|(^|[^A-Za-z0-9_])g_bus_get(_sync)?[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])g_dbus_proxy_new(_sync|_for_bus|_for_bus_sync)?[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])GDBusConnection([^A-Za-z0-9_]|$))"
    "alternate service proxy"
)
reject_linux_secret_service_surface(
    "(GObjectConstructParam|g_param_spec_get_name|g_value_set_string|\"g-name\"|(^|[^A-Za-z0-9_])g_object_set(_property)?[ \t\r\n]*\\()"
    "ineffective service-name property override"
)
reject_linux_secret_service_surface(
    "(g_ascii_strcasecmp|g_str_has_(prefix|suffix)|(^|[^A-Za-z0-9_])(strcasecmp|strncasecmp|strncmp|strstr)[ \t\r\n]*\\(|\\.starts_with[ \t\r\n]*\\(|\\.ends_with[ \t\r\n]*\\(|\\.contains[ \t\r\n]*\\(|\"(text|application)/\\*\"|\"\\*/\\*\")"
    "broad SecretValue content-type bypass"
)
reject_linux_secret_service_surface(
    "(QtKeychain|QKeychain|KWallet|kwallet|(^|[^A-Za-z0-9_])gnome_keyring_[A-Za-z0-9_]*[ \t\r\n]*\\(|libgnome-keyring|secret-tool|PasswordVault|PasswordCredential)"
    "alternate credential vault"
)
reject_linux_secret_service_surface(
    "(#[ \t]*include[ \t]*<(filesystem|fstream)>|std::filesystem|std::([io]?fstream|[io]stream)|(^|[^A-Za-z0-9_.:])(fopen|freopen|fread|fwrite|open|openat|creat|read|write|pread|pwrite|g_file_(get|set)_contents|g_(open|fopen|mkdir|mkdir_with_parents|remove|rename|rmdir))[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(GFile|GIOChannel)([^A-Za-z0-9_]|$))"
    "filesystem"
)
reject_linux_secret_service_surface(
    "([Ss][Qq][Ll][Ii][Tt][Ee]3?|QSettings|GSettings|g_settings_|GKeyFile|g_key_file_|dconf)"
    "settings/database"
)
reject_linux_secret_service_surface(
    "(QProcess|GSubprocess|g_subprocess_|(^|[^A-Za-z0-9_])(g_spawn[A-Za-z0-9_]*|posix_spawn|system|popen|fork|exec(l|le|lp|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "command/process"
)
reject_linux_secret_service_surface(
    "(QNetwork[A-Za-z0-9_]*|QtNetwork|G(Socket|Resolver|NetworkAddress|InetAddress|Tls)[A-Za-z0-9_]*|g_(socket|resolver|network_address|inet_address|tls)_[A-Za-z0-9_]*|SoupSession|libcurl|#[ \t]*include[ \t]*[<\"]curl/|CURL[A-Za-z0-9_]*|(^|[^A-Za-z0-9_])(socket|connect|send|recv)[ \t\r\n]*\\(|https?://)"
    "network"
)
reject_linux_secret_service_surface(
    "(#[ \t]*include[ \t]*[<\"](Q|Qt|gtk/)|(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*|GtkWidget|GtkWindow|GtkDialog|gtk_[A-Za-z0-9_]*)([^A-Za-z0-9_]|$))"
    "Qt/UI"
)
reject_linux_secret_service_surface(
    "(std::(cout|cerr|clog)|(^|[^A-Za-z0-9_])(printf|fprintf|puts|fputs|perror|syslog|g_log|g_debug|g_info|g_message|g_warning|g_critical|g_print|g_printerr|qDebug|qCDebug|qInfo|qCInfo|qWarning|qCWarning|qCritical|qCCritical|qFatal)[ \t\r\n]*\\(|spdlog)"
    "logging/printing"
)
reject_linux_secret_service_surface(
    "(g_dbus_error_get_remote_error|g_dbus_error_strip_remote_error|g_quark_to_string|(^|[^A-Za-z0-9_])(GError[ \t]*\\*[^;\r\n]*->[ \t]*message|error->[ \t]*message)|#[ \t]*include[ \t]*<format>|std::(format|vformat|to_string)|([io]stringstream|snprintf|sprintf)[ \t\r\n]*\\()"
    "raw operating-system error formatting"
)
reject_linux_secret_service_surface(
    "(^|[^A-Za-z0-9_])[Ff]allback([^A-Za-z0-9_]|$)"
    "credential fallback"
)

foreach(
    exact_singleton
    IN ITEMS
        "g_dbus_proxy_call_sync"
)
    string(
        REGEX MATCHALL
        "${exact_singleton}"
        linux_secret_service_singletons
        "${linux_secret_service_contents}"
    )
    list(
        LENGTH
        linux_secret_service_singletons
        linux_secret_service_singleton_count
    )
    if(NOT linux_secret_service_singleton_count EQUAL 1)
        message(
            FATAL_ERROR
            "Linux Secret Service adapter must retain exactly one bounded "
            "Dismiss call"
        )
    endif()
endforeach()

set(
    linux_secret_service_selected_effects
    "${linux_secret_service_contents}"
)
foreach(
    selected_effect
    IN ITEMS
        secret_collection_for_alias_sync
        secret_collection_get_locked
        secret_collection_search_sync
        secret_item_create_sync
        secret_item_delete_sync
        secret_item_get_attributes
        secret_item_get_locked
        secret_item_get_secret
        secret_item_load_secret_sync
        secret_service_ensure_session_sync
        secret_service_open_sync
        secret_value_get_content_type
        secret_value_get
        secret_value_new
        secret_value_owner
        secret_value_unref
)
    string(
        REPLACE
        "${selected_effect}"
        ""
        linux_secret_service_selected_effects
        "${linux_secret_service_selected_effects}"
    )
endforeach()
if(
    linux_secret_service_selected_effects
        MATCHES
            "(^|[^A-Za-z0-9_])secret_(collection|item|password|schema|service|value)_[A-Za-z0-9_]*[ \t\r\n]*\\("
)
    message(
        FATAL_ERROR
        "Linux Secret Service adapter uses a libsecret effect outside the "
        "selected open, default-collection, search, binary item lifecycle"
    )
endif()

set(
    alternate_process_patterns
    "(NSTask|GSubprocess|g_subprocess_|ShellExecute(Ex)?[AW]?|WinExec|posix_spawnp|(^|[^A-Za-z0-9_])(_?w?system|_?w?popen|fork|vfork|clone3?|exec(l|le|lp|v|ve|vp|vpe)|_?w?spawn(l|le|lp|lpe|v|ve|vp|vpe))[ \t\r\n]*\\()"
    "(#[ \t]*include[ \t]*[<\"]boost/process|boost::process|Poco::Process|wx(Process|Execute)|uv_spawn|g_spawn[A-Za-z0-9_]*)"
    "(/bin/(ba|z|k|c)?sh|\"(/usr/bin/)?(sh|bash|zsh|ksh|csh|fish|cmd(\\.exe)?|powershell(\\.exe)?|pwsh(\\.exe)?)\"|(^|[^A-Za-z0-9_])(sh|bash|zsh|cmd)[ \t]+(-c|/c)([^A-Za-z0-9_]|$))"
)
foreach(source_file IN LISTS source_files)
    file(READ "${source_file}" contents)
    file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
    if(relative_file STREQUAL agent_unix_supervisor_relative)
        # This bundled owner may fork one direct child and execute the exact
        # argv supplied by the application. Other process/shell APIs stay banned.
        string(REPLACE "::fork()" "" contents "${contents}")
        string(REPLACE "::execv(argv[1], argv + 1)" "" contents "${contents}")
    endif()
    if(relative_file STREQUAL "src/adapters/qt/src/painted_glyph_icon.cpp")
        # [[BPR-1296]]. QIconEngine's required copy override is named clone;
        # only that exact declaration is removed, not the Linux process call.
        string(REPLACE "auto clone() const -> QIconEngine* override" "" contents "${contents}")
    endif()
    foreach(alternate_process_pattern IN LISTS alternate_process_patterns)
        if(contents MATCHES "${alternate_process_pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "Alternate process or shell API is forbidden in "
                "${relative_file}: ${alternate_process_pattern}"
            )
        endif()
    endforeach()
endforeach()

# BPR-0894: four of the variables reserved below are also the standard way a
# platform publishes a per-user directory, and reserving the *token* stopped
# any other component from reading one to find that directory. The boundary
# this rule means to defend is narrower: the GitHub CLI transport owns
# *constructing* the child process environment, including the decision about
# which variables are passed through to the child. Reading one as a directory
# path -- through the `environment_directory("NAME")` accessor, which yields an
# absolute path or nothing -- is discovery, not construction, and is permitted
# anywhere. Every other spelling is still the transport's alone: a bare
# literal, an assignment, or a name appended to a child environment all still
# fail, which is the property worth defending.
#
# The remaining reserved names are deliberately not in this set. HOME and PATH
# already have three named owners and are more than directories -- they
# redirect a child tool's whole notion of state. DBUS_SESSION_BUS_ADDRESS is a
# bus address rather than a directory, and the Linux Secret Service adapter is
# separately forbidden from naming it on purpose. HTTPS_PROXY, NO_PROXY,
# SSL_CERT_FILE and SSL_CERT_DIR are network and TLS-trust policy: a component
# that read SSL_CERT_DIR would be making a trust decision, not finding a
# directory, which is exactly the leak this rule exists to catch.
set(
    platform_directory_environment_variables
    "XDG_RUNTIME_DIR"
    "USERPROFILE"
    "APPDATA"
    "LOCALAPPDATA"
)

function(
    verify_environment_construction_owner
    variable_name
    effect_name
    allowed_relative_file
)
    set(allowed_relative_files "${allowed_relative_file}" ${ARGN})
    set(allowed_files "")
    foreach(current_allowed_relative IN LISTS allowed_relative_files)
        get_filename_component(
            current_allowed
            "${BPR_ROOT}/${current_allowed_relative}"
            ABSOLUTE
        )
        list(APPEND allowed_files "${current_allowed}")
    endforeach()

    foreach(source_file IN LISTS source_files)
        get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
        if(normalized_source_file IN_LIST allowed_files)
            continue()
        endif()

        file(READ "${source_file}" contents)
        set(contents_before_discovery_reads "${contents}")
        # The accessor spelling is anchored. Without the leading class any name
        # *ending* in `environment_directory` would be stripped too, and the
        # transport's own convention already contains
        # `canonical_environment_directory`, so
        # `child.inherit_environment_directory("APPDATA")` would have laundered
        # a real leak. `.`, `>` and `:` are excluded as well, so a member or
        # qualified call on some other object cannot pose as the accessor.
        string(
            REGEX REPLACE
            "(^|[^A-Za-z0-9_.>:])environment_directory[ \t\r\n]*\\([ \t\r\n]*\"${variable_name}\"[ \t\r\n]*\\)"
            "\\1"
            contents
            "${contents}"
        )
        if(NOT contents STREQUAL contents_before_discovery_reads)
            # This file reads the variable for discovery. The spelling alone is
            # not a guarantee -- the accessor is file-local, so a file could
            # define its own `environment_directory` that constructs rather than
            # reads. A file may discover, or it may build child environments,
            # and only the transport may do both.
            if(
                contents
                    MATCHES
                    "(append_environment[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])(set|put)env[ \t\r\n]*\\(|QProcessEnvironment|\\.Environment|(^|[^A-Za-z0-9_])environ([^A-Za-z0-9_]|$))"
            )
                file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
                message(
                    FATAL_ERROR
                    "${effect_name} ownership violation in ${relative_file}; "
                    "it reads ${variable_name} for directory discovery and "
                    "also constructs a child process environment. Only "
                    "${allowed_relative_files} may do both"
                )
            endif()
        endif()
        if(contents MATCHES "\"${variable_name}\"")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
            message(
                FATAL_ERROR
                "${effect_name} ownership violation in ${relative_file}; "
                "only ${allowed_relative_files} may use it. Reading "
                "${variable_name} as a directory through "
                "environment_directory(\"${variable_name}\") is discovery "
                "rather than construction and is allowed anywhere"
            )
        endif()
    endforeach()
endfunction()

set(
    github_cli_invocation_and_environment_patterns
    "\"api\""
    "\"--hostname\""
    "\"--method\""
    "\"--input\""
    "GH_PROMPT_DISABLED=1"
    "GH_NO_UPDATE_NOTIFIER=1"
    "GH_NO_EXTENSION_UPDATE_NOTIFIER=1"
    "GH_SPINNER_DISABLED=1"
    "NO_COLOR=1"
    "CLICOLOR=0"
    "\"HOME\""
    "\"XDG_RUNTIME_DIR\""
    "\"DBUS_SESSION_BUS_ADDRESS\""
    "\"USERPROFILE\""
    "\"APPDATA\""
    "\"LOCALAPPDATA\""
    "\"HTTPS_PROXY\""
    "\"NO_PROXY\""
    "\"SSL_CERT_FILE\""
    "\"SSL_CERT_DIR\""
)
foreach(
    cli_construction_pattern
    IN LISTS github_cli_invocation_and_environment_patterns
)
    set(platform_directory_variable "")
    foreach(
        candidate_directory_variable
        IN LISTS platform_directory_environment_variables
    )
        if(cli_construction_pattern STREQUAL "\"${candidate_directory_variable}\"")
            set(platform_directory_variable "${candidate_directory_variable}")
        endif()
    endforeach()

    if(
        cli_construction_pattern STREQUAL "\"HOME\""
        OR cli_construction_pattern STREQUAL "\"PATH\""
    )
        verify_effect_owner(
            "${cli_construction_pattern}"
            "process environment construction"
            "${github_cli_transport_relative}"
            "${git_process_runner_relative}"
            "${agent_profile_policy_relative}"
        )
    elseif(cli_construction_pattern STREQUAL "\"SystemRoot\"")
        verify_effect_owner(
            "${cli_construction_pattern}"
            "process environment construction"
            "${github_cli_transport_relative}"
            "${git_process_runner_relative}"
        )
    elseif(NOT platform_directory_variable STREQUAL "")
        verify_environment_construction_owner(
            "${platform_directory_variable}"
            "GitHub CLI invocation/environment construction"
            "${github_cli_transport_relative}"
        )
    else()
        verify_effect_owner(
            "${cli_construction_pattern}"
            "GitHub CLI invocation/environment construction"
            "${github_cli_transport_relative}"
        )
    endif()
endforeach()

set(
    github_cli_runner_environment_validation_patterns
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"PATH\"[ \t\r\n]*\\)"
    "equal_environment_names[ \t\r\n]*\\([ \t\r\n]*entry\\.Name[ \t\r\n]*,[ \t\r\n]*\"SystemRoot\"[ \t\r\n]*\\)"
)
set(
    github_process_runner_without_environment_validation
    "${github_process_runner_contents}"
)
foreach(
    runner_environment_validation_pattern
    IN LISTS github_cli_runner_environment_validation_patterns
)
    string(
        REGEX REPLACE
        "${runner_environment_validation_pattern}"
        ""
        github_process_runner_without_environment_validation
        "${github_process_runner_without_environment_validation}"
    )
endforeach()
if(
    github_process_runner_without_environment_validation
        MATCHES "\"(PATH|SystemRoot)\""
)
    message(
        FATAL_ERROR
        "GitHub CLI runner may name PATH and SystemRoot only while validating "
        "the transport-constructed Windows environment"
    )
endif()

foreach(source_file IN LISTS source_files)
    get_filename_component(normalized_source_file "${source_file}" ABSOLUTE)
    file(READ "${source_file}" contents)
    if(
        contents MATCHES "\"(HOME|PATH|SystemRoot)\""
        AND NOT normalized_source_file STREQUAL github_cli_transport_file
        AND NOT normalized_source_file STREQUAL github_process_runner_file
        AND NOT normalized_source_file STREQUAL git_process_runner_file
        AND NOT (
            normalized_source_file STREQUAL agent_profile_policy_file
            AND contents MATCHES "\"(HOME|PATH)\""
            AND NOT contents MATCHES "\"SystemRoot\""
        )
    )
        file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source_file}")
        message(
            FATAL_ERROR
            "GitHub CLI invocation/environment construction ownership "
            "violation in ${relative_file}; HOME, PATH, and SystemRoot are owned by "
            "${github_cli_transport_relative}, with validation only in "
            "${github_process_runner_relative}, or by the isolated installed-"
            "Git runner ${git_process_runner_relative}"
        )
    endif()

    file(
        RELATIVE_PATH
        relative_source_file
        "${BPR_ROOT}"
        "${source_file}"
    )
    if(
        relative_source_file MATCHES "^src/adapters/github/"
        AND contents MATCHES "\"\\.git\""
        AND NOT normalized_source_file STREQUAL github_cli_transport_file
        AND NOT normalized_source_file STREQUAL github_process_runner_file
    )
        message(
            FATAL_ERROR
            "GitHub CLI neutral working-directory validation ownership "
            "violation in ${relative_source_file}; only "
            "${github_cli_transport_relative} and "
            "${github_process_runner_relative} may validate it"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE runtime_files
    LIST_DIRECTORIES false
    "${BPR_ROOT}/CMakeLists.txt"
    "${BPR_ROOT}/cmake/*.cmake"
    "${BPR_ROOT}/src/*/CMakeLists.txt"
    "${BPR_ROOT}/src/*.c"
    "${BPR_ROOT}/src/*.cc"
    "${BPR_ROOT}/src/*.cpp"
    "${BPR_ROOT}/src/*.cxx"
    "${BPR_ROOT}/src/*.h"
    "${BPR_ROOT}/src/*.hh"
    "${BPR_ROOT}/src/*.hpp"
    "${BPR_ROOT}/src/*.hxx"
    "${BPR_ROOT}/src/*.m"
    "${BPR_ROOT}/src/*.mm"
)
set(
    runtime_forbidden_patterns
    "WebEngine"
    "Chromium"
    "Electron"
    "Tauri"
    "QTcpServer"
    "QUdpSocket"
    "QLocalServer"
    "QPluginLoader"
    "QWeb"
    "listen[ \t]*\\("
    "auto[_-]?update"
)

foreach(runtime_file IN LISTS runtime_files)
    if(runtime_file STREQUAL CMAKE_CURRENT_LIST_FILE)
        continue()
    endif()

    file(READ "${runtime_file}" contents)
    file(RELATIVE_PATH runtime_relative "${BPR_ROOT}" "${runtime_file}")
    if(runtime_relative STREQUAL "src/adapters/sqlite/src/profile_activation.cpp")
        # RFC-0024 admits one lease-bound local activation listener, not a
        # general network service or an exception for the adapter directory.
        if(NOT contents MATCHES "AF_UNIX" OR contents MATCHES "AF_INET|sockaddr_in")
            message(FATAL_ERROR "Profile activation must use only local Unix sockets")
        endif()
        string(REPLACE "::listen(listener.Value, 8)" "" contents "${contents}")
    endif()
    foreach(pattern IN LISTS runtime_forbidden_patterns)
        if(contents MATCHES "${pattern}")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${runtime_file}")
            message(
                FATAL_ERROR
                "Forbidden runtime surface in ${relative_file}: ${pattern}"
            )
        endif()
    endforeach()
endforeach()

set(telemetry_pattern "[Tt][Ee][Ll][Ee][Mm][Ee][Tt][Rr][Yy]")
get_filename_component(
    github_cli_transport
    "${BPR_ROOT}/${github_cli_transport_relative}"
    ABSOLUTE
)
foreach(runtime_file IN LISTS runtime_files)
    if(runtime_file STREQUAL CMAKE_CURRENT_LIST_FILE)
        continue()
    endif()

    file(READ "${runtime_file}" contents)
    if(NOT contents MATCHES "${telemetry_pattern}")
        continue()
    endif()

    get_filename_component(normalized_runtime_file "${runtime_file}" ABSOLUTE)
    # [[BPR-1375]]. Tool-free writing turns Claude Code's built-in reporting
    # plugin off by name; that exact setting is hardening, as GH_TELEMETRY=0 is.
    if(normalized_runtime_file STREQUAL "${BPR_ROOT}/src/application/include/bpr/application/agent_launch.hpp")
        string(
            REPLACE
            "\"cc-plugin-telemetry@builtin\":false"
            ""
            contents_without_hardening
            "${contents}"
        )
        if(NOT contents_without_hardening MATCHES "${telemetry_pattern}")
            continue()
        endif()
    endif()
    if(normalized_runtime_file STREQUAL github_cli_transport)
        string(
            REPLACE
            "GH_TELEMETRY=0"
            ""
            contents_without_hardening
            "${contents}"
        )
        if(NOT contents_without_hardening MATCHES "${telemetry_pattern}")
            continue()
        endif()
    endif()

    file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${runtime_file}")
    message(
        FATAL_ERROR
        "Forbidden telemetry surface in ${relative_file}; only the exact "
        "GH_TELEMETRY=0 broker hardening setting is allowed"
    )
endforeach()

if(EXISTS "${github_cli_transport}")
    file(READ "${github_cli_transport}" github_cli_transport_contents)
    if(NOT github_cli_transport_contents MATCHES "GH_TELEMETRY=0")
        message(
            FATAL_ERROR
            "The designated GitHub CLI transport must set GH_TELEMETRY=0"
        )
    endif()
endif()

# [[BPR-1343]]. A Qt adapter local, parameter or capture named after a data
# member a Qt base class carries hides it. MSVC reports C4458, the project
# builds with warnings as errors, and Windows stops compiling, while Clang and
# GCC say nothing: [[BPR-1090]]'s and [[BPR-1336]]'s `data` each broke main.
# Only data members warn (a local named `size` or `width` hides a member
# function and is silent), so the list is exactly the data members widget code
# can meet: QWidget::data and QObject::d_ptr. A method named `data(...)`, a
# model's override, is a function and is allowed. Comments, character and
# string literals name nothing; a qualified name or a statement keyword before
# the name is a use, not a declaration.
set(qt_shadowed_members data d_ptr)
file(GLOB_RECURSE qt_shadow_sources "${BPR_ROOT}/src/adapters/qt/*.cpp" "${BPR_ROOT}/src/adapters/qt/*.hpp")
foreach(source IN LISTS qt_shadow_sources)
    file(READ "${source}" qt_shadow_code)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" " " qt_shadow_code "${qt_shadow_code}")
    string(REGEX REPLACE "//[^\n]*" " " qt_shadow_code "${qt_shadow_code}")
    string(REGEX REPLACE "'([^'\\\\\n]|\\\\.)'" " " qt_shadow_code "${qt_shadow_code}")
    string(REGEX REPLACE "\"([^\"\\\\\n]|\\\\.)*\"" " " qt_shadow_code "${qt_shadow_code}")
    string(REGEX REPLACE "(^|[^A-Za-z0-9_])(return|throw|co_return|co_yield|case|else|delete)[ \t]+" "\\1; " qt_shadow_code "${qt_shadow_code}")
    foreach(qt_shadowed IN LISTS qt_shadowed_members)
        string(REPLACE "${qt_shadowed}::" "qualified_name::" qt_shadow_probe "${qt_shadow_code}")
        if(qt_shadow_probe MATCHES "[A-Za-z0-9_>&*][ \t\n]+[&*]*${qt_shadowed}[ \t]*[=;,){:]"
           OR qt_shadow_probe MATCHES "[[,][ \t]*[&=]?[ \t]*${qt_shadowed}[ \t]*[],=]")
            file(RELATIVE_PATH relative_file "${BPR_ROOT}" "${source}")
            message(
                FATAL_ERROR
                "Qt adapter declaration hides a Qt base class data member: "
                "${relative_file} names a local, parameter or capture "
                "`${qt_shadowed}`, which MSVC reports as C4458 and the Windows "
                "build refuses. Rename it (see src/adapters/qt/AGENTS.md)."
            )
        endif()
    endforeach()
endforeach()

message(STATUS "Application, provider, and Qt architecture boundaries verified")

file(GLOB_RECURSE quill_boundary_sources "${BPR_ROOT}/src/*.cpp" "${BPR_ROOT}/src/*.hpp")
foreach(source IN LISTS quill_boundary_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "#[ \t]*include[ \t]*[<\"]quill/" AND NOT source MATCHES "/src/adapters/logging/")
        message(FATAL_ERROR "Quill effects belong exclusively to the logging adapter: ${source}")
    endif()
endforeach()
