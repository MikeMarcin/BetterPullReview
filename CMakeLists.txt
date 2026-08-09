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
    VERSION 0.1.0
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
option(
    BPR_DEPENDENCY_OFFLINE
    "Require all CPM-managed source dependencies to exist in CPM_SOURCE_CACHE"
    OFF
)

include("${CMAKE_ROOT}/Modules/CTest.cmake")
option(
    BPR_ENABLE_CODING_TARGETS
    "Enable BetterPullReview coding-standard utility targets"
    "${BUILD_TESTING}"
)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Hardening.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Warnings.cmake")
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
    endif()
endfunction()

add_subdirectory(third_party/sqlite)

add_subdirectory(src/core)
add_subdirectory(src/application)
add_subdirectory(src/adapters)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
