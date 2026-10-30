# [[BPR-1832]]. The compiler-cache policy in cmake/BprCompilerCache.cmake:
# every AUTO, ON and OFF branch, the launcher it builds from
# cmake/bpr-ccache.conf, and, when run from a configured graph, that the
# graph's launcher matches the decision configure recorded.
#
#   cmake -DBPR_SOURCE_DIR=<checkout>
#         [-DBPR_CONFIGURED_STATE=<state> -DBPR_CONFIGURED_LAUNCHER=<a|b|…>
#          -DBPR_CONFIGURED_C_LAUNCHER=<a|b|…> -DBPR_CONFIGURED_CXX_LAUNCHER=<a|b|…>]
#         -P tests/cmake/compiler_cache_tests.cmake

cmake_minimum_required(VERSION 4.4.2)

if(NOT BPR_SOURCE_DIR)
    message(FATAL_ERROR "BPR_SOURCE_DIR is required")
endif()
# The module declares its cache option; script mode has no cache, so this
# include only defines the functions under test.
include("${BPR_SOURCE_DIR}/cmake/BprCompilerCache.cmake")

set_property(GLOBAL PROPERTY bpr_compiler_cache_failures 0)
function(fail text)
    message(SEND_ERROR "${text}")
    get_property(count GLOBAL PROPERTY bpr_compiler_cache_failures)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY bpr_compiler_cache_failures "${count}")
endfunction()

# expect_decision(<expected> <mode> [KEY=VALUE]...): every input starts as a
# Debug Ninja AppleClang development graph with ccache 4.14.2 present, the
# case AUTO is for, and each KEY=VALUE replaces one input (VALUE may be
# empty). An <expected> ending in `*` compares only its prefix.
function(expect_decision expected mode)
    set(PROGRAM /opt/ccache)
    set(VERSION 4.14.2)
    set(DISTRIBUTION OFF)
    set(GENERATOR Ninja)
    set(C_COMPILER_ID AppleClang)
    set(CXX_COMPILER_ID AppleClang)
    set(BUILD_TYPE Debug)
    set(EXISTING_LAUNCHER "")
    foreach(override IN LISTS ARGN)
        if(NOT override MATCHES "^([A-Z_]+)=(.*)$")
            message(FATAL_ERROR "not KEY=VALUE: ${override}")
        endif()
        set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    endforeach()
    bpr_compiler_cache_decision(
        actual
        MODE "${mode}"
        PROGRAM "${PROGRAM}"
        VERSION "${VERSION}"
        DISTRIBUTION "${DISTRIBUTION}"
        GENERATOR "${GENERATOR}"
        C_COMPILER_ID "${C_COMPILER_ID}"
        CXX_COMPILER_ID "${CXX_COMPILER_ID}"
        BUILD_TYPE "${BUILD_TYPE}"
        EXISTING_LAUNCHER "${EXISTING_LAUNCHER}"
    )
    set(ok FALSE)
    if(expected MATCHES "\\*$")
        string(REGEX REPLACE "\\*$" "" prefix "${expected}")
        string(FIND "${actual}" "${prefix}" at)
        if(at EQUAL 0)
            set(ok TRUE)
        endif()
    elseif(actual STREQUAL expected)
        set(ok TRUE)
    endif()
    if(NOT ok)
        fail("decision ${mode} ${ARGN}\n  expected: ${expected}\n  actual:   ${actual}")
    endif()
endfunction()

# Applied when present.
expect_decision("enabled" AUTO)
expect_decision("enabled" auto)
expect_decision("enabled" ON)
expect_decision("enabled" AUTO C_COMPILER_ID=GNU CXX_COMPILER_ID=GNU "GENERATOR=Unix Makefiles")
expect_decision("enabled" ON BUILD_TYPE=Release)

# Opted out.
expect_decision("skipped: BPR_COMPILER_CACHE=OFF" OFF)
expect_decision("skipped: BPR_COMPILER_CACHE=OFF" OFF DISTRIBUTION=ON)
expect_decision("error: BPR_COMPILER_CACHE must be*" maybe)

# Absent: AUTO builds as before, ON refuses.
expect_decision("skipped: ccache was not found on PATH" AUTO PROGRAM=)
expect_decision("skipped: ccache was not found on PATH" AUTO PROGRAM=BPR_CCACHE_PROGRAM-NOTFOUND)
expect_decision("error: BPR_COMPILER_CACHE=ON, but ccache was not found on PATH" ON PROGRAM=)
expect_decision("skipped: ccache 4.7.4 is older than 4.8" AUTO VERSION=4.7.4)

# Release, qualification and signing graphs are left as they were.
expect_decision("skipped: a distribution graph never compiles through a cache" AUTO DISTRIBUTION=ON)
expect_decision("error: BPR_COMPILER_CACHE=ON, but a distribution graph*" ON DISTRIBUTION=ON BUILD_TYPE=Release)
expect_decision("skipped: AUTO covers Debug development graphs*" AUTO BUILD_TYPE=Release)
expect_decision("skipped: AUTO covers Debug development graphs*" AUTO BUILD_TYPE=)

# Graphs the launcher cannot reach, and a launcher the caller chose.
expect_decision("skipped: the Visual Studio 18 2026 generator ignores compiler launchers" AUTO "GENERATOR=Visual Studio 18 2026")
expect_decision("skipped: the Xcode generator*" AUTO GENERATOR=Xcode)
expect_decision("skipped: the MSVC compiler is not configured*" AUTO C_COMPILER_ID=MSVC CXX_COMPILER_ID=MSVC)
expect_decision("skipped: CMAKE_<LANG>_COMPILER_LAUNCHER is already set to 'sccache'" AUTO EXISTING_LAUNCHER=sccache)

# The launcher: ccache, the checkout-specific settings, then the shipped file.
bpr_compiler_cache_launcher(
    launcher
    PROGRAM /opt/ccache
    CONFIG "${BPR_SOURCE_DIR}/cmake/bpr-ccache.conf"
    CACHE_DIR "/home/me/.cache/bpr-compiler-cache"
    BASE_DIR "/work/tree"
)
set(expected_launcher /opt/ccache cache_dir=/home/me/.cache/bpr-compiler-cache base_dir=/work/tree max_size=20G)
if(NOT launcher STREQUAL expected_launcher)
    fail("launcher\n  expected: ${expected_launcher}\n  actual:   ${launcher}")
endif()
bpr_compiler_cache_launcher(
    launcher
    APPLE_CLONE
    PROGRAM /opt/ccache
    CONFIG "${BPR_SOURCE_DIR}/cmake/bpr-ccache.conf"
    CACHE_DIR "/c"
    BASE_DIR "/b"
)
if(NOT launcher MATCHES ";file_clone=true;compression=false$")
    fail("an APFS launcher must clone uncompressed entries: ${launcher}")
endif()

# The default cache directory is outside the checkout.
bpr_compiler_cache_default_directory(directory)
cmake_path(IS_PREFIX BPR_SOURCE_DIR "${directory}" NORMALIZE inside)
if(inside OR NOT directory MATCHES "/bpr-compiler-cache$")
    fail("default cache directory ${directory} must be bpr-compiler-cache outside ${BPR_SOURCE_DIR}")
endif()

# The configured graph, when the test runs from one. Its lists arrive
# `|`-joined (tests/CMakeLists.txt).
if(DEFINED BPR_CONFIGURED_STATE)
    foreach(name IN ITEMS BPR_CONFIGURED_LAUNCHER BPR_CONFIGURED_C_LAUNCHER BPR_CONFIGURED_CXX_LAUNCHER)
        string(REPLACE "|" ";" "${name}" "${${name}}")
    endforeach()
    if(BPR_CONFIGURED_STATE STREQUAL "enabled")
        list(GET BPR_CONFIGURED_LAUNCHER 0 program)
        if(NOT EXISTS "${program}" OR NOT program MATCHES "ccache")
            fail("an enabled cache must launch an existing ccache, not '${program}'")
        endif()
        if(NOT BPR_CONFIGURED_C_LAUNCHER STREQUAL BPR_CONFIGURED_LAUNCHER
           OR NOT BPR_CONFIGURED_CXX_LAUNCHER STREQUAL BPR_CONFIGURED_LAUNCHER)
            fail("the C and C++ launchers must both be the configured ccache launcher")
        endif()
        if(NOT "base_dir=${BPR_SOURCE_DIR}" IN_LIST BPR_CONFIGURED_LAUNCHER)
            fail("the launcher must set base_dir to the checkout: ${BPR_CONFIGURED_LAUNCHER}")
        endif()
    elseif(BPR_CONFIGURED_STATE MATCHES "^skipped: ")
        if(NOT BPR_CONFIGURED_LAUNCHER STREQUAL "")
            fail("a skipped cache must leave no launcher: ${BPR_CONFIGURED_LAUNCHER}")
        endif()
        if(BPR_CONFIGURED_C_LAUNCHER MATCHES "ccache" OR BPR_CONFIGURED_CXX_LAUNCHER MATCHES "ccache")
            fail("a skipped cache must not reach the compiler launchers")
        endif()
    else()
        fail("unexpected configured state '${BPR_CONFIGURED_STATE}'")
    endif()
    message(STATUS "configured graph: ${BPR_CONFIGURED_STATE}")
endif()

get_property(count GLOBAL PROPERTY bpr_compiler_cache_failures)
if(count GREATER 0)
    message(FATAL_ERROR "${count} compiler-cache contract check(s) failed")
endif()
message(STATUS "COMPILER_CACHE_CONTRACT_OK")
