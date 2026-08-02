cmake_minimum_required(VERSION 4.4.2)

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

option(BPR_BUILD_QT "Build the Qt presentation adapter" ON)
option(
    BPR_DEPENDENCY_OFFLINE
    "Require cached CPM content and an existing valid CPS dependency overlay"
    OFF
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CTest)
include(Hardening)
include(Warnings)
include(BprDependencies)
include(BprSQLiteProvider)

bpr_configure_dependencies()
bpr_acquire_sqlite(BPR_SQLITE_SOURCE_DIR)

add_subdirectory(third_party/sqlite)

add_subdirectory(src/core)
add_subdirectory(src/application)
add_subdirectory(src/adapters)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
