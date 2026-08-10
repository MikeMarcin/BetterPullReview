include_guard(GLOBAL)

set(
    BPR_MACOS_TEST_SANDBOX_PROFILE
    "${PROJECT_SOURCE_DIR}/cmake/sandbox/macos-network-deny.sb"
)
set(
    BPR_MACOS_TEST_SANDBOX_RUNNER
    "${PROJECT_SOURCE_DIR}/scripts/run_network_isolated_test.py"
)

if(APPLE)
    if(NOT EXISTS "/usr/bin/sandbox-exec")
        message(
            FATAL_ERROR
            "Automated tests require /usr/bin/sandbox-exec on macOS; refusing an unsandboxed test graph"
        )
    endif()
    if(NOT EXISTS "${BPR_MACOS_TEST_SANDBOX_PROFILE}")
        message(
            FATAL_ERROR
            "Automated-test sandbox profile is missing: ${BPR_MACOS_TEST_SANDBOX_PROFILE}"
        )
    endif()
    if(NOT EXISTS "${BPR_MACOS_TEST_SANDBOX_RUNNER}")
        message(
            FATAL_ERROR
            "Automated-test sandbox runner is missing: ${BPR_MACOS_TEST_SANDBOX_RUNNER}"
        )
    endif()
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "macos-sandbox-exec")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    bpr_add_executable(
        tool_better_pull_review_network_sandbox
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/linux_network_sandbox.cpp"
    )
    target_compile_features(
        tool_better_pull_review_network_sandbox
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(tool_better_pull_review_network_sandbox)
    bpr_enable_warnings(tool_better_pull_review_network_sandbox)
    set(
        BPR_AUTOMATED_TEST_ISOLATION_PREFIX
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "linux-network-namespace")
elseif(WIN32)
    set(
        BPR_AUTOMATED_TEST_ISOLATION_PREFIX
        "${CMAKE_COMMAND}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/FailClosedAutomatedTestIsolation.cmake"
    )
    set(BPR_AUTOMATED_TEST_ISOLATION_BACKEND "windows-unavailable")
else()
    message(
        FATAL_ERROR
        "No default-deny automated-test network boundary exists for ${CMAKE_SYSTEM_NAME}"
    )
endif()

function(bpr_add_network_isolated_test)
    cmake_parse_arguments(
        PARSE_ARGV 0
        bpr_test
        "COMMAND_EXPAND_LISTS;UNRELATED_ENDPOINT_PROBE"
        "NAME;WORKING_DIRECTORY;LOOPBACK_PORT_COUNT"
        "COMMAND;CONFIGURATIONS"
    )
    if(bpr_test_UNPARSED_ARGUMENTS)
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test received unsupported arguments: ${bpr_test_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(NOT bpr_test_NAME OR NOT bpr_test_COMMAND)
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test requires NAME and COMMAND"
        )
    endif()
    if(NOT bpr_test_LOOPBACK_PORT_COUNT)
        set(bpr_test_LOOPBACK_PORT_COUNT 0)
    endif()
    if(NOT bpr_test_LOOPBACK_PORT_COUNT MATCHES "^[0-8]$")
        message(
            FATAL_ERROR
            "bpr_add_network_isolated_test LOOPBACK_PORT_COUNT must be between 0 and 8"
        )
    endif()
    if(bpr_test_UNRELATED_ENDPOINT_PROBE AND NOT APPLE)
        message(
            FATAL_ERROR
            "UNRELATED_ENDPOINT_PROBE currently has target-native evidence only on macOS"
        )
    endif()

    list(POP_FRONT bpr_test_COMMAND bpr_test_executable)
    if(TARGET "${bpr_test_executable}")
        set(bpr_test_executable "$<TARGET_FILE:${bpr_test_executable}>")
    endif()

    set(bpr_registration_options "")
    if(bpr_test_CONFIGURATIONS)
        list(
            APPEND
            bpr_registration_options
            CONFIGURATIONS
            ${bpr_test_CONFIGURATIONS}
        )
    endif()
    if(bpr_test_WORKING_DIRECTORY)
        list(
            APPEND
            bpr_registration_options
            WORKING_DIRECTORY
            "${bpr_test_WORKING_DIRECTORY}"
        )
    endif()
    if(bpr_test_COMMAND_EXPAND_LISTS)
        list(APPEND bpr_registration_options COMMAND_EXPAND_LISTS)
    endif()

    if(APPLE)
        set(
            bpr_isolation_prefix
            "$<TARGET_FILE:Python3::Interpreter>"
            -I
            -S
            "${BPR_MACOS_TEST_SANDBOX_RUNNER}"
            --sandbox-exec
            /usr/bin/sandbox-exec
            --profile-template
            "${BPR_MACOS_TEST_SANDBOX_PROFILE}"
            --loopback-port-count
            "${bpr_test_LOOPBACK_PORT_COUNT}"
        )
        if(bpr_test_UNRELATED_ENDPOINT_PROBE)
            list(APPEND bpr_isolation_prefix --unrelated-endpoint-probe)
        endif()
        list(APPEND bpr_isolation_prefix --)
    else()
        set(bpr_isolation_prefix ${BPR_AUTOMATED_TEST_ISOLATION_PREFIX})
    endif()

    add_test(
        NAME "${bpr_test_NAME}"
        COMMAND
            ${bpr_isolation_prefix}
            "${bpr_test_executable}"
            ${bpr_test_COMMAND}
        ${bpr_registration_options}
    )
    set_property(
        TEST "${bpr_test_NAME}"
        APPEND
        PROPERTY LABELS "bpr_network_isolated"
    )
    set_property(
        TEST "${bpr_test_NAME}"
        APPEND
        PROPERTY ENVIRONMENT_MODIFICATION
            "PYTHONPATH=unset:"
            "PYTHONHOME=unset:"
            "PYTHONUSERBASE=unset:"
            "PYTHONINSPECT=unset:"
            "PYTHONSTARTUP=unset:"
            "DYLD_INSERT_LIBRARIES=unset:"
            "DYLD_LIBRARY_PATH=unset:"
            "DYLD_FRAMEWORK_PATH=unset:"
            "DYLD_VERSIONED_LIBRARY_PATH=unset:"
            "DYLD_VERSIONED_FRAMEWORK_PATH=unset:"
            "DYLD_FALLBACK_LIBRARY_PATH=unset:"
            "DYLD_FALLBACK_FRAMEWORK_PATH=unset:"
            "DYLD_IMAGE_SUFFIX=unset:"
            "DYLD_ROOT_PATH=unset:"
            "DYLD_SHARED_REGION=unset:"
            "LD_PRELOAD=unset:"
            "LD_LIBRARY_PATH=unset:"
            "LD_AUDIT=unset:"
            "LD_DEBUG=unset:"
            "LD_PROFILE=unset:"
    )
    if(bpr_test_LOOPBACK_PORT_COUNT GREATER 0)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_loopback_fixture"
        )
    endif()
    if(bpr_test_UNRELATED_ENDPOINT_PROBE)
        set_property(
            TEST "${bpr_test_NAME}"
            APPEND
            PROPERTY LABELS "bpr_unrelated_endpoint_probe"
        )
    endif()
endfunction()
