include_guard(GLOBAL)

# Windows offers an unprivileged process no network namespace, so the
# default-deny automated-test boundary is one fixed AppContainer entered with
# zero capabilities. Two machine facts have to hold before that boundary can
# carry a test suite, and a contained child can establish neither of them:
#
#   * the container has to read the sources, the resolved Qt prefix, and the
#     built executables, and to write the build tree. CreateProcessW reports
#     success even when the container cannot read the image it was handed and
#     the launch then fails incoherently much later, so the grants must exist
#     before any test is registered;
#   * loopback has to be exempted for the container. Adding that exemption
#     needs elevation, which a build must never take on the owner's behalf, so
#     a missing entry is reported as the one manual step it is.
#
# Both are settled here, at configure time. A missing prerequisite refuses the
# build instead of producing a suite that silently cannot run.

set(BPR_WINDOWS_APPCONTAINER_NAME "BprAutomatedTest")

file(TO_CMAKE_PATH "$ENV{SystemRoot}" bpr_windows_system_root)
set(BPR_WINDOWS_ICACLS "${bpr_windows_system_root}/System32/icacls.exe")
set(
    BPR_WINDOWS_CHECK_NET_ISOLATION
    "${bpr_windows_system_root}/System32/CheckNetIsolation.exe"
)
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
function(bpr_windows_appcontainer_sid launcher_source output)
    set(attempt_limit 3)
    foreach(attempt RANGE 1 ${attempt_limit})
        try_run(
            bpr_appcontainer_setup_status
            BPR_APPCONTAINER_SETUP_COMPILED
            SOURCES "${launcher_source}"
            LINK_LIBRARIES userenv advapi32
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED TRUE
            COMPILE_OUTPUT_VARIABLE setup_build_log
            RUN_OUTPUT_STDOUT_VARIABLE setup_output
            RUN_OUTPUT_STDERR_VARIABLE setup_error
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

# The exemption is machine state an unprivileged build cannot create, so its
# absence is a refusal that names the exact elevated command instead of a
# guess about why every test later failed to reach its own fixtures.
function(bpr_windows_appcontainer_require_loopback_exemption sid)
    execute_process(
        COMMAND "${BPR_WINDOWS_CHECK_NET_ISOLATION}" LoopbackExempt -s
        RESULT_VARIABLE query_status
        OUTPUT_VARIABLE query_output
        ERROR_VARIABLE query_error
    )
    if(NOT query_status EQUAL 0)
        message(
            FATAL_ERROR
            "Unable to read the Windows loopback exemption list "
            "(status ${query_status}): ${query_error}"
        )
    endif()

    string(TOUPPER "${query_output}" exempted)
    string(TOUPPER "${sid}" wanted_sid)
    string(TOUPPER "${BPR_WINDOWS_APPCONTAINER_NAME}" wanted_name)
    string(FIND "${exempted}" "${wanted_sid}" sid_offset)
    string(FIND "${exempted}" "${wanted_name}" name_offset)
    if(sid_offset GREATER_EQUAL 0 OR name_offset GREATER_EQUAL 0)
        return()
    endif()

    message(
        FATAL_ERROR
        "============================================================\n"
        "ONE ELEVATED COMMAND IS REQUIRED BEFORE TESTS CAN BE BUILT\n"
        "============================================================\n"
        "Windows blocks loopback for an AppContainer until the machine\n"
        "carries a loopback exemption for it, and BetterPullReview's\n"
        "automated tests reach their own local fixtures over loopback.\n"
        "This build refuses rather than register a suite that could\n"
        "only fail, and it never elevates on your behalf.\n"
        "Open a Command Prompt or PowerShell AS ADMINISTRATOR and run\n"
        "this exact command once, then configure again:\n"
        "    CheckNetIsolation.exe LoopbackExempt -a"
        " -n=${BPR_WINDOWS_APPCONTAINER_NAME}\n"
        "Confirm it took effect with:\n"
        "    CheckNetIsolation.exe LoopbackExempt -s\n"
        "AppContainer : ${BPR_WINDOWS_APPCONTAINER_NAME}\n"
        "Container SID: ${sid}\n"
        "This is the only manual step in the Windows automated-test\n"
        "network boundary.\n"
        "============================================================"
    )
endfunction()

# Establish every machine prerequisite the AppContainer boundary depends on,
# or refuse the configure.
function(bpr_configure_windows_test_isolation launcher_source)
    foreach(
        tool
        IN ITEMS
            "${BPR_WINDOWS_ICACLS}"
            "${BPR_WINDOWS_CHECK_NET_ISOLATION}"
    )
        if(NOT EXISTS "${tool}")
            message(
                FATAL_ERROR
                "The Windows automated-test network boundary requires ${tool}"
            )
        endif()
    endforeach()

    bpr_windows_appcontainer_sid("${launcher_source}" container_sid)

    # The exemption is checked before anything is granted. It is a read-only
    # query, and refusing after having rewritten access control on the owner's
    # source tree would leave a side effect behind for a build that never ran.
    bpr_windows_appcontainer_require_loopback_exemption("${container_sid}")

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
