include_guard(GLOBAL)

set(
    BPR_MACOS_TEST_SANDBOX_PROFILE
    "${PROJECT_SOURCE_DIR}/cmake/sandbox/macos-network-deny.sb"
)
set(
    BPR_MACOS_TEST_SANDBOX_RUNNER
    "${PROJECT_SOURCE_DIR}/scripts/run_network_isolated_test.py"
)
option(
    BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST
    "Register the single pre-launch-validated designated live-mutation CTest lane"
    OFF
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
    ""
    CACHE FILEPATH
    "Absolute path to the one-shot designated live-mutation approval artifact"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR
    ""
    CACHE FILEPATH
    "Absolute path to the signed-capsule designated live supervisor"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_INVENTORY
    ""
    CACHE FILEPATH
    "Absolute path to the signed-capsule inventory"
)
set(
    BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
    ""
    CACHE PATH
    "Absolute build directory that produced the designated shipped application"
)
if(BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
    if(NOT BPR_BUILD_QT)
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires BPR_BUILD_QT=ON"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute non-symlink BPR_DESIGNATED_LIVE_MUTATION_APPROVAL file"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        OR IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        OR NOT IS_EXECUTABLE "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR executable"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_INVENTORY
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
        OR IS_DIRECTORY "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an existing absolute BPR_DESIGNATED_LIVE_MUTATION_INVENTORY file"
        )
    endif()
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
        OR NOT IS_ABSOLUTE "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        OR NOT EXISTS "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}/CMakeCache.txt"
        OR IS_SYMLINK "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
    )
        message(
            FATAL_ERROR
            "BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST requires an absolute BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR with CMakeCache.txt"
        )
    endif()

    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        bpr_designated_live_supervisor_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
        bpr_designated_live_inventory_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        bpr_designated_live_build_real
    )
    file(
        REAL_PATH
        "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
        bpr_designated_live_approval_real
    )
    if(
        NOT BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR
            STREQUAL bpr_designated_live_supervisor_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_INVENTORY
            STREQUAL bpr_designated_live_inventory_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR
            STREQUAL bpr_designated_live_build_real
        OR NOT BPR_DESIGNATED_LIVE_MUTATION_APPROVAL
            STREQUAL bpr_designated_live_approval_real
    )
        message(
            FATAL_ERROR
            "Designated live supervisor, inventory, producer build, and approval paths must already be resolved absolute paths"
        )
    endif()

    get_filename_component(
        bpr_designated_live_supervisor_name
        "${bpr_designated_live_supervisor_real}"
        NAME
    )
    string(TOLOWER
        "${bpr_designated_live_supervisor_name}"
        bpr_designated_live_supervisor_name_lower
    )
    if(
        NOT bpr_designated_live_supervisor_name_lower
            STREQUAL "better_pull_review_designated_live_supervisor"
    )
        message(
            FATAL_ERROR
            "Designated live command must be the signed-capsule BetterPullReview supervisor, never the regression runner, application, or a fixture"
        )
    endif()
    cmake_path(
        IS_PREFIX
        bpr_designated_live_build_real
        "${bpr_designated_live_supervisor_real}"
        NORMALIZE
        bpr_designated_live_supervisor_in_build
    )
    cmake_path(
        IS_PREFIX
        bpr_designated_live_build_real
        "${bpr_designated_live_inventory_real}"
        NORMALIZE
        bpr_designated_live_inventory_in_build
    )
    if(
        NOT bpr_designated_live_supervisor_in_build
        OR NOT bpr_designated_live_inventory_in_build
    )
        message(
            FATAL_ERROR
            "Designated live supervisor and inventory must be inside their exact producer build directory"
        )
    endif()

    file(
        REAL_PATH
        "${PROJECT_SOURCE_DIR}"
        bpr_designated_live_source_real
    )
    file(
        STRINGS
        "${bpr_designated_live_build_real}/CMakeCache.txt"
        bpr_designated_live_producer_cache
    )
    foreach(
        bpr_designated_live_required_cache
        IN ITEMS
            "BUILD_TESTING:BOOL=OFF"
            "BPR_BUILD_QT:BOOL=ON"
            "CMAKE_BUILD_TYPE:STRING=Release"
            "CMAKE_HOME_DIRECTORY:INTERNAL=${bpr_designated_live_source_real}"
    )
        list(FIND
            bpr_designated_live_producer_cache
            "${bpr_designated_live_required_cache}"
            bpr_designated_live_required_cache_index
        )
        if(bpr_designated_live_required_cache_index EQUAL -1)
            message(
                FATAL_ERROR
                "Designated live producer cache must contain exactly ${bpr_designated_live_required_cache}"
            )
        endif()
    endforeach()

    file(
        GLOB_RECURSE
        bpr_designated_live_product_inputs
        LIST_DIRECTORIES false
        "${PROJECT_SOURCE_DIR}/src/*"
        "${PROJECT_SOURCE_DIR}/cmake/*"
    )
    list(
        APPEND
        bpr_designated_live_product_inputs
        "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
        "${PROJECT_SOURCE_DIR}/CMakePresets.json"
    )
    file(
        TIMESTAMP
        "${bpr_designated_live_supervisor_real}"
        bpr_designated_live_supervisor_timestamp
        "%Y%m%d%H%M%S"
        UTC
    )
    foreach(bpr_designated_live_product_input IN LISTS bpr_designated_live_product_inputs)
        file(
            TIMESTAMP
            "${bpr_designated_live_product_input}"
            bpr_designated_live_product_input_timestamp
            "%Y%m%d%H%M%S"
            UTC
        )
        if(
            bpr_designated_live_product_input_timestamp
                STRGREATER bpr_designated_live_supervisor_timestamp
        )
            message(
                FATAL_ERROR
                "Designated live supervisor predates product source or build configuration: ${bpr_designated_live_product_input}"
            )
        endif()
    endforeach()
endif()

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

function(bpr_add_designated_live_mutation_test)
    if(ARGC GREATER 0)
        message(
            FATAL_ERROR
            "bpr_add_designated_live_mutation_test accepts no caller-selected name, target, or command"
        )
    endif()
    if(NOT BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
        return()
    endif()
    add_test(
        NAME live_review_mutation_designated_target
        COMMAND
            "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
            --attended-launch
            --inventory
            "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
            --approval
            "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
    )
    set_property(
        TEST live_review_mutation_designated_target
        PROPERTY LABELS "bpr_live_mutation"
    )
    set_property(
        TEST live_review_mutation_designated_target
        PROPERTY TIMEOUT 1800
    )
    set_property(
        TEST live_review_mutation_designated_target
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
            "GH_TOKEN=unset:"
            "GITHUB_TOKEN=unset:"
            "GH_ENTERPRISE_TOKEN=unset:"
            "GITHUB_ENTERPRISE_TOKEN=unset:"
    )
endfunction()
