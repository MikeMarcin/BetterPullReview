include_guard(GLOBAL)

# Windows offers an unprivileged process no network namespace, so the
# default-deny automated-test boundary is one fixed AppContainer entered with
# zero capabilities. One machine fact has to hold before that boundary can
# carry a test suite, and a contained child cannot establish it: the container
# has to read the sources, the resolved Qt prefix, and the built executables,
# and to write the build tree. CreateProcessW reports success even when the
# container cannot read the image it was handed and the launch then fails
# incoherently much later, so the grants must exist before any test is
# registered. That is settled here, at configure time, and a missing
# prerequisite refuses the build instead of producing a suite that silently
# cannot run.
#
# This deliberately does not ask for a machine-wide loopback exemption. The
# restriction AppContainers carry is on reaching a loopback listener owned by
# something else; two processes inside the same container reach each other over
# loopback with no exemption at all. Every fixture in this suite is created by
# the test that uses it, and CTest launches the whole test through the launcher,
# so the listener and its client are always in this one container together.
# Requiring the exemption would have demanded an elevated command from the
# owner in exchange for authority the suite never exercises, and refused the
# whole build until it was granted. `test_network_isolation_loopback` proves
# the channel positively at test time, which is where the claim belongs.

set(BPR_WINDOWS_APPCONTAINER_NAME "BprAutomatedTest")

file(TO_CMAKE_PATH "$ENV{SystemRoot}" bpr_windows_system_root)
set(BPR_WINDOWS_ICACLS "${bpr_windows_system_root}/System32/icacls.exe")
unset(bpr_windows_system_root)

# The container SID is a hash of the container name that only the Windows API
# computes, and configure time needs it before any target has been built. The
# launcher source therefore answers one exact single-argument setup query,
# which also creates the profile on a machine that has never had it.
#
# A freshly linked executable is occasionally unrunnable for a moment while a
# scanner holds it, and that reports as FAILED_TO_RUN rather than as a refusal
# by the setup itself. Exit status is therefore not the evidence: each attempt
# is judged by the SID it printed, in the same shape the pinned Qt provider
# already uses. A compile failure is never transient and fails immediately.
function(bpr_windows_appcontainer_sid launcher_source output launcher_output)
    set(setup_launcher "${CMAKE_BINARY_DIR}/CMakeFiles/bpr-appcontainer-setup.exe")
    set(attempt_limit 3)
    foreach(attempt RANGE 1 ${attempt_limit})
        try_run(
            bpr_appcontainer_setup_status
            BPR_APPCONTAINER_SETUP_COMPILED
            SOURCES "${launcher_source}"
            LINK_LIBRARIES userenv advapi32 ole32
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED TRUE
            COMPILE_OUTPUT_VARIABLE setup_build_log
            RUN_OUTPUT_STDOUT_VARIABLE setup_output
            RUN_OUTPUT_STDERR_VARIABLE setup_error
            COPY_FILE "${setup_launcher}"
            ARGS --bpr-appcontainer-setup
        )
        if(NOT BPR_APPCONTAINER_SETUP_COMPILED)
            message(
                FATAL_ERROR
                "Unable to build the Windows AppContainer launcher from "
                "'${launcher_source}':\n${setup_build_log}"
            )
        endif()
        if(setup_output MATCHES "BPR_APPCONTAINER_SID=(S-1-15-2-[0-9-]+)")
            set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            set(${launcher_output} "${setup_launcher}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(
        FATAL_ERROR
        "Unable to establish the AppContainer profile "
        "'${BPR_WINDOWS_APPCONTAINER_NAME}' after ${attempt_limit} attempts. "
        "Last status ${bpr_appcontainer_setup_status}, output '${setup_output}', "
        "error '${setup_error}'"
    )
endfunction()

# The first process is created by the uncontained launcher. Its descendant
# must be able to open the interpreter itself, which a per-user installation
# does not normally grant a lowbox token. Probe that exact runtime before
# touching its ACL: a machine-wide installation may already admit the
# container, and the developer need not own that installation to use it.
function(bpr_windows_appcontainer_python sid launcher)
    if(NOT TARGET Python3::Interpreter)
        return()
    endif()
    get_target_property(python_executable Python3::Interpreter IMPORTED_LOCATION)
    file(REAL_PATH "${python_executable}" python_executable)
    get_filename_component(python_runtime "${python_executable}" DIRECTORY)
    set(probe "import subprocess,sys; subprocess.run([sys.executable,'-I','-B','-c','pass'],check=True,capture_output=True,text=True,timeout=10)")
    execute_process(
        COMMAND "${launcher}" "${python_executable}" -I -B -c "${probe}"
        RESULT_VARIABLE probe_status
        OUTPUT_VARIABLE probe_log
        ERROR_VARIABLE probe_log
        WORKING_DIRECTORY "${python_runtime}"
        TIMEOUT 15
    )
    if(probe_status STREQUAL "0")
        return()
    endif()
    bpr_windows_appcontainer_grant("${sid}" "(OI)(CI)(RX)" "${python_runtime}")
    execute_process(
        COMMAND "${launcher}" "${python_executable}" -I -B -c "${probe}"
        RESULT_VARIABLE probe_status
        OUTPUT_VARIABLE probe_log
        ERROR_VARIABLE probe_log
        WORKING_DIRECTORY "${python_runtime}"
        TIMEOUT 15
    )
    if(NOT probe_status STREQUAL "0")
        message(FATAL_ERROR "The selected Python runtime cannot start a contained descendant: ${probe_log}")
    endif()
    message(STATUS "bpr: AppContainer ${BPR_WINDOWS_APPCONTAINER_NAME} reads the selected Python runtime ${python_runtime}")
endfunction()

# A contained process is not the invoking user, so it reaches the project only
# through an explicit ACE. `:r` replaces this SID's own explicit entry and
# leaves every other principal alone, which keeps repeated configures exactly
# idempotent. The inheritable flags reach existing children through the
# auto-inheritance the call already performs, so no per-file walk is asked for.
function(bpr_windows_appcontainer_grant sid access path)
    if(NOT IS_DIRECTORY "${path}")
        message(
            FATAL_ERROR
            "Cannot grant AppContainer access to a missing directory: ${path}"
        )
    endif()
    file(TO_NATIVE_PATH "${path}" native_path)
    execute_process(
        COMMAND
            "${BPR_WINDOWS_ICACLS}" "${native_path}" /grant:r
            "*${sid}:${access}" /Q
        RESULT_VARIABLE grant_status
        OUTPUT_VARIABLE grant_log
        ERROR_VARIABLE grant_log
    )
    if(NOT grant_status EQUAL 0 OR grant_log MATCHES "Failed processing [1-9]")
        message(
            FATAL_ERROR
            "Unable to grant AppContainer '${BPR_WINDOWS_APPCONTAINER_NAME}' "
            "${access} on ${native_path}: ${grant_log}"
        )
    endif()
endfunction()

# Establish every machine prerequisite the AppContainer boundary depends on,
# or refuse the configure.
function(bpr_configure_windows_test_isolation launcher_source)
    if(NOT EXISTS "${BPR_WINDOWS_ICACLS}")
        message(
            FATAL_ERROR
            "The Windows automated-test network boundary requires ${BPR_WINDOWS_ICACLS}"
        )
    endif()

    bpr_windows_appcontainer_sid("${launcher_source}" container_sid setup_launcher)
    bpr_windows_appcontainer_python("${container_sid}" "${setup_launcher}")

    # Sources, the resolved Qt prefix, and the built executables are read; the
    # build tree is also written, because tests keep their scratch data there.
    # The Qt prefix is a separate scope only when the provider resolved it
    # outside the project, which BPR_QT_ROOT allows.
    set(read_scopes "${PROJECT_SOURCE_DIR}")
    if(Qt6_DIR)
        get_filename_component(qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        cmake_path(
            IS_PREFIX
            PROJECT_SOURCE_DIR
            "${qt_prefix}"
            NORMALIZE
            qt_prefix_inside_project
        )
        if(IS_DIRECTORY "${qt_prefix}" AND NOT qt_prefix_inside_project)
            list(APPEND read_scopes "${qt_prefix}")
        endif()
    endif()

    foreach(scope IN LISTS read_scopes)
        bpr_windows_appcontainer_grant("${container_sid}" "(OI)(CI)(RX)" "${scope}")
    endforeach()
    bpr_windows_appcontainer_grant("${container_sid}" "(OI)(CI)(M)" "${CMAKE_BINARY_DIR}")
    string(REPLACE ";" ", " read_scope_text "${read_scopes}")
    message(
        STATUS
        "bpr: AppContainer ${BPR_WINDOWS_APPCONTAINER_NAME} (${container_sid}) "
        "reads ${read_scope_text} and writes ${CMAKE_BINARY_DIR}"
    )

endfunction()
