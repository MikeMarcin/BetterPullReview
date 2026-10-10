cmake_minimum_required(VERSION 4.4.2)

if(CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
    message(
        FATAL_ERROR
        "BetterPullReview does not permit CMAKE_PROJECT_TOP_LEVEL_INCLUDES; "
        "top-level dependency providers can replace audited dependencies"
    )
endif()

if(DEFINED BPR_BUILD_TESTS)
    message(
        FATAL_ERROR
        "BPR_BUILD_TESTS was removed. Use the standard BUILD_TESTING option "
        "(-DBUILD_TESTING=ON or -DBUILD_TESTING=OFF)."
    )
endif()

project(
    BetterPullReview
    VERSION 0.1.7
    DESCRIPTION "Native GitHub pull-request review workbench"
    HOMEPAGE_URL "https://github.com/MikeMarcin/BetterPullReview"
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
    message(
        FATAL_ERROR
        "BetterPullReview does not permit CMAKE_PROJECT_TOP_LEVEL_INCLUDES; "
        "top-level dependency providers can replace audited dependencies"
    )
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    message(
        FATAL_ERROR
        "BetterPullReview must be configured as the top-level project so "
        "dependency providers cannot intercept audited package discovery"
    )
endif()

option(BPR_BUILD_QT "Build the Qt presentation adapter" ON)
option(BPR_BUILD_TOUR_CLI "Build the non-GUI bpr tour terminal frontend" OFF)
# [[BPR-1179]]. Two removable component families (RFC-BPR-0060). Agent
# integrations are the agent launch adapter, the application's agent hosts, the
# MCP context helper and the desktop surfaces that drive them; without them the
# desktop composes no agent and ships no agent helper. Local automation is the
# bpr command line and the desktop's owner command channel it reaches; without
# it the desktop opens no command channel and the graph builds no bpr. Each is
# independent of the other, and neither touches stored reviews or cycles.
option(BPR_BUILD_AGENT_INTEGRATIONS "Build the agent launch adapter, agent hosts and MCP context helper" ON)
option(BPR_BUILD_LOCAL_AUTOMATION "Build the bpr command line and the desktop's owner command channel" ON)
if(BPR_BUILD_TOUR_CLI AND NOT BPR_BUILD_LOCAL_AUTOMATION)
    message(FATAL_ERROR "BPR_BUILD_TOUR_CLI builds bpr, which is local automation: it needs BPR_BUILD_LOCAL_AUTOMATION=ON")
endif()
option(
    BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE
    "Build the narrow tests-disabled macOS attended-review execution capsule"
    OFF
)
if(BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE AND NOT APPLE)
    message(
        FATAL_ERROR
        "BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE is supported only on macOS"
    )
endif()
if(BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE AND BUILD_TESTING)
    message(
        FATAL_ERROR
        "The designated live macOS capsule requires BUILD_TESTING=OFF"
    )
endif()
option(
    BPR_DEPENDENCY_OFFLINE
    "Require all CPM-managed source dependencies to exist in CPM_SOURCE_CACHE"
    OFF
)

include("${CMAKE_ROOT}/Modules/CTest.cmake")
# [[BPR-0970]]. An opt-in graph for the parser and mapping fuzz targets in
# tests/fuzz. Every first-party unit is instrumented, not only the harnesses:
# libFuzzer steers by the coverage of the production parser it calls, and
# AddressSanitizer can only see that parser's accesses if it was compiled for
# them. The flags are set here, before any first-party target exists.
option(BPR_BUILD_FUZZERS "Build the libFuzzer parser and mapping targets (Clang on macOS or Linux)" OFF)
if(BPR_BUILD_FUZZERS)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "BPR_BUILD_FUZZERS requires BUILD_TESTING=ON")
    endif()
    if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
        message(
            FATAL_ERROR
            "BPR_BUILD_FUZZERS needs Clang or Apple clang on macOS or Linux "
            "(see docs/development/build-and-test.md)"
        )
    endif()
    # -O1 keeps a campaign fast enough to matter; line tables keep its stacks
    # readable. Both follow the libFuzzer documentation's recommendation.
    set(BPR_FUZZ_INSTRUMENTATION
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
        -O1
        -gline-tables-only
    )
    list(JOIN BPR_FUZZ_INSTRUMENTATION " " bpr_fuzz_flags)
    string(APPEND CMAKE_C_FLAGS " ${bpr_fuzz_flags}")
    string(APPEND CMAKE_CXX_FLAGS " ${bpr_fuzz_flags}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -fsanitize=address,undefined")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fsanitize=address,undefined")
endif()
option(BPR_DISTRIBUTION_BUILD "Restrict the product to its deployed runtime" OFF)
if(BPR_DISTRIBUTION_BUILD AND (BUILD_TESTING OR NOT BPR_BUILD_QT))
    message(FATAL_ERROR "Distribution builds require the tests-disabled application graph")
endif()
option(
    BPR_ENABLE_CODING_TARGETS
    "Enable BetterPullReview coding-standard utility targets"
    "${BUILD_TESTING}"
)
# The publisher the Windows installer and version resources name.
set(BPR_VENDOR_NAME "GameCoreTech")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hardening.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Warnings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BprWindowsImage.cmake")
# Before the dependencies: whether the syntax definitions are embedded in
# KSyntaxHighlighting depends on where this graph keeps its Qt resources.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BprQtResources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BprDependencies.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BprSQLiteProvider.cmake")

bpr_configure_dependencies()
include("${CMAKE_CURRENT_SOURCE_DIR}/_coding-standard/cmake/CodingStandard.cmake")
bpr_acquire_sqlite(BPR_SQLITE_SOURCE_DIR)

function(bpr_add_executable target)
    if(NOT target MATCHES "^(app|bench|demo|fuzz|test|tool)_")
        message(
            FATAL_ERROR
            "First-party executable target '${target}' requires a coding-standard category prefix"
        )
    endif()

    add_executable("${target}" ${ARGN})
    string(REGEX REPLACE "^[^_]+_" "" output_name "${target}")
    set_target_properties("${target}" PROPERTIES OUTPUT_NAME "${output_name}")
    if(MSVC)
        # The Git cache's ref layout crosses MAX_PATH by design; Git writes
        # through the extended-length form on its own, and every first-party
        # process that walks those paths with std::filesystem needs the
        # manifest opt-in for the Win32 APIs to do the same (BPR-0437).
        target_sources(
            "${target}"
            PRIVATE
                "${PROJECT_SOURCE_DIR}/cmake/bpr_long_paths.manifest"
        )
    endif()

    # Every first-party executable passes through here, which is the only
    # place that can enumerate them without a directory walk guessing at the
    # naming convention. Non-production targets are collected so
    # `live_mutation_isolation` can prove none of them links a live GitHub
    # mutation. A future bench_, demo_, fuzz_, or tool_ target is covered the
    # day it is added, because it has to be added here.
    if(NOT target MATCHES "^app_")
        set_property(
            GLOBAL
            APPEND
            PROPERTY BPR_AUTOMATED_EXECUTABLES "${target}"
        )
        # The Debug CRT reports abort() and failed assertions with a modal box
        # that waits for a person. Every automated executable gets the unit
        # that routes those to stderr instead, so a failure is an exit code
        # CTest can act on rather than a dialog an unattended run stalls on
        # (BPR-0434). Shipped tools are also non-application executables, but
        # must not inherit test-only CRT/crash policy in a distribution graph.
        if(MSVC AND BUILD_TESTING)
            target_sources(
                "${target}"
                PRIVATE
                    "${PROJECT_SOURCE_DIR}/tests/msvc_quiet_crt_reports.cpp"
            )
        endif()
    endif()
endfunction()

if(BUILD_TESTING)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BprTestIsolation.cmake")
endif()

add_subdirectory(third_party/sqlite)

add_subdirectory(src/core)
add_subdirectory(src/application)
add_subdirectory(src/platform)
add_subdirectory(src/adapters)
add_subdirectory(src/app)
if(BPR_DISTRIBUTION_BUILD)
    include("${CMAKE_CURRENT_SOURCE_DIR}/packaging/Install.cmake")
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
