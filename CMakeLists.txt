cmake_minimum_required(VERSION 3.25)

project(
    BetterPullReview
    VERSION 0.1.0
    DESCRIPTION "Native GitHub pull-request review workbench"
    LANGUAGES C CXX
)

option(BPR_BUILD_QT "Build the Qt presentation adapter" ON)
option(BPR_BUILD_TESTS "Build and register BetterPullReview tests" ON)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Hardening)
include(Warnings)

add_subdirectory(third_party/sqlite)

add_subdirectory(src/core)
add_subdirectory(src/application)
add_subdirectory(src/adapters)
add_subdirectory(src/app)

if(BPR_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
