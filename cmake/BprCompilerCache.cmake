# [[BPR-1832]]. An optional shared ccache for development graphs.
#
# Agents build in many worktrees at once (.claude/worktrees/*,
# ~/.codex/worktrees/*), and each used to compile every unit from nothing.
# When ccache 4.8 or newer is on PATH, a Debug development graph compiles
# through it, and every checkout, worktree and preset on the machine shares one
# capped cache outside all of them (docs/development/build-and-test.md,
# "Shared compiler cache").
#
# BPR_COMPILER_CACHE selects the policy:
#   AUTO (default) uses ccache when it is installed and the graph is a Debug,
#        non-distribution Ninja or Makefile graph built by Clang or GCC, and
#        otherwise builds exactly as it would without ccache.
#   ON   requires it, for any non-distribution graph, and fails configure when
#        it cannot be used.
#   OFF  never uses it.
# A distribution graph (macos-release, windows-app-release, linux-package and
# their variants) never uses it: base_dir rewrites absolute paths to relative
# ones, which changes __FILE__ and debug paths in the objects, and a release's
# bytes must not depend on what is installed on the building machine. An
# explicit CMAKE_<LANG>_COMPILER_LAUNCHER is left alone.
#
# Cross-worktree hits: base_dir is the checkout's own source directory, so the
# compiler sees paths inside it, including the CPM cache under build/, as
# relative to the build directory, and those are the same in every worktree.
# A Debug unit's DWARF also names the compile directory, so the build
# directory is mapped to `.` with -fdebug-prefix-map; ccache does not hash that
# option and hashes the mapped directory instead.

set(
    BPR_COMPILER_CACHE
    "AUTO"
    CACHE STRING
    "Compile through ccache: AUTO (when installed, Debug development graphs), ON or OFF"
)
set_property(CACHE BPR_COMPILER_CACHE PROPERTY STRINGS AUTO ON OFF)

set(BPR_COMPILER_CACHE_MINIMUM_VERSION "4.8")

# Decide from explicit inputs only, so tests/cmake/compiler_cache_tests.cmake
# can drive every branch in script mode. Sets <output> to `enabled`,
# `skipped: <reason>` or `error: <reason>`.
function(bpr_compiler_cache_decision output)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        arg
        ""
        "MODE;PROGRAM;VERSION;DISTRIBUTION;GENERATOR;C_COMPILER_ID;CXX_COMPILER_ID;BUILD_TYPE;EXISTING_LAUNCHER"
        ""
    )
    string(TOUPPER "${arg_MODE}" mode)
    if(NOT mode MATCHES "^(AUTO|ON|OFF)$")
        set("${output}" "error: BPR_COMPILER_CACHE must be AUTO, ON or OFF, not '${arg_MODE}'" PARENT_SCOPE)
        return()
    endif()
    if(mode STREQUAL "OFF")
        set("${output}" "skipped: BPR_COMPILER_CACHE=OFF" PARENT_SCOPE)
        return()
    endif()

    set(reason "")
    if(arg_DISTRIBUTION)
        set(reason "a distribution graph never compiles through a cache")
    elseif(NOT "${arg_EXISTING_LAUNCHER}" STREQUAL "")
        set(reason "CMAKE_<LANG>_COMPILER_LAUNCHER is already set to '${arg_EXISTING_LAUNCHER}'")
    elseif(NOT arg_GENERATOR MATCHES "^(Ninja|Ninja Multi-Config|.*Makefiles)$")
        set(reason "the ${arg_GENERATOR} generator ignores compiler launchers")
    elseif(
        NOT arg_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$"
        OR NOT arg_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$"
    )
        set(reason "the ${arg_CXX_COMPILER_ID} compiler is not configured for the cache (Clang and GCC are)")
    elseif(NOT arg_PROGRAM)
        set(reason "ccache was not found on PATH")
    elseif(arg_VERSION VERSION_LESS BPR_COMPILER_CACHE_MINIMUM_VERSION)
        set(reason "ccache ${arg_VERSION} is older than ${BPR_COMPILER_CACHE_MINIMUM_VERSION}")
    elseif(mode STREQUAL "AUTO" AND NOT arg_BUILD_TYPE STREQUAL "Debug")
        set(reason "AUTO covers Debug development graphs; pass -DBPR_COMPILER_CACHE=ON for a ${arg_BUILD_TYPE} graph")
    endif()

    if(reason STREQUAL "")
        set("${output}" "enabled" PARENT_SCOPE)
    elseif(mode STREQUAL "ON")
        set("${output}" "error: BPR_COMPILER_CACHE=ON, but ${reason}" PARENT_SCOPE)
    else()
        set("${output}" "skipped: ${reason}" PARENT_SCOPE)
    endif()
endfunction()

# The default cache directory: the platform's per-user cache root, never
# inside a checkout, and a name of its own so that it cannot meet the
# application's own QStandardPaths cache (…/BetterPullReview).
function(bpr_compiler_cache_default_directory output)
    if(APPLE)
        set(root "$ENV{HOME}/Library/Caches")
    elseif(WIN32)
        file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" root)
    elseif(NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(root "$ENV{XDG_CACHE_HOME}")
    else()
        set(root "$ENV{HOME}/.cache")
    endif()
    set("${output}" "${root}/bpr-compiler-cache" PARENT_SCOPE)
endfunction()

# The launcher: ccache, then each setting as `key=value`, which ccache 4.8 and
# newer reads before the compiler and lets win over its configuration files.
function(bpr_compiler_cache_launcher output)
    cmake_parse_arguments(PARSE_ARGV 1 arg "APPLE_CLONE" "PROGRAM;CONFIG;CACHE_DIR;BASE_DIR" "")
    set(launcher "${arg_PROGRAM}" "cache_dir=${arg_CACHE_DIR}" "base_dir=${arg_BASE_DIR}")
    file(STRINGS "${arg_CONFIG}" lines)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        if(NOT line MATCHES "^([a-z_]+)[ \t]*=[ \t]*(.*)$")
            message(FATAL_ERROR "${arg_CONFIG}: not a `key = value` line: ${line}")
        endif()
        set(key "${CMAKE_MATCH_1}")
        set(value "${CMAKE_MATCH_2}")
        if(key MATCHES "^(cache_dir|base_dir)$")
            message(FATAL_ERROR "${arg_CONFIG}: ${key} is set by configure, not by the file")
        endif()
        list(APPEND launcher "${key}=${value}")
    endforeach()
    if(arg_APPLE_CLONE)
        # APFS clones a hit into the build tree, so N worktrees hold one copy;
        # ccache clones only uncompressed entries.
        list(APPEND launcher "file_clone=true" "compression=false")
    endif()
    set("${output}" "${launcher}" PARENT_SCOPE)
endfunction()

# The compile command line carries every setting, but a maintenance command
# (`CCACHE_DIR=<dir> ccache -s`, `-c`, `-X`) reads only <dir>/ccache.conf, and
# without it would assume ccache's default 5 GB cap and evict down to it. Write
# the launcher's settings, less cache_dir and base_dir, there too. Several
# configures may race, so each writes a private file and renames it into place.
function(bpr_compiler_cache_write_directory_config directory launcher)
    set(text "# Written by BetterPullReview's configure from cmake/bpr-ccache.conf ([[BPR-1832]]).\n")
    list(POP_FRONT launcher)
    foreach(setting IN LISTS launcher)
        if(NOT setting MATCHES "^(cache_dir|base_dir)=")
            string(REGEX REPLACE "^([a-z_]+)=" "\\1 = " setting "${setting}")
            string(APPEND text "${setting}\n")
        endif()
    endforeach()
    set(path "${directory}/ccache.conf")
    if(EXISTS "${path}")
        file(READ "${path}" current)
        if(current STREQUAL text)
            return()
        endif()
    endif()
    file(MAKE_DIRECTORY "${directory}")
    string(RANDOM LENGTH 12 suffix)
    file(WRITE "${path}.${suffix}" "${text}")
    file(RENAME "${path}.${suffix}" "${path}")
endfunction()

function(bpr_configure_compiler_cache)
    find_program(BPR_CCACHE_PROGRAM NAMES ccache DOC "ccache for BPR_COMPILER_CACHE")
    bpr_compiler_cache_default_directory(default_directory)
    set(
        BPR_COMPILER_CACHE_DIR
        "${default_directory}"
        CACHE PATH
        "The shared ccache directory; keep it outside every checkout"
    )

    # A cached path that has since been uninstalled counts as absent.
    set(program "${BPR_CCACHE_PROGRAM}")
    if(program AND NOT EXISTS "${program}")
        set(program "")
    endif()
    set(version "")
    if(program)
        execute_process(
            COMMAND "${program}" --version
            OUTPUT_VARIABLE version_text
            RESULT_VARIABLE version_result
            ERROR_QUIET
        )
        if(version_result EQUAL 0 AND version_text MATCHES "ccache version ([0-9]+(\\.[0-9]+)*)")
            set(version "${CMAKE_MATCH_1}")
        else()
            set(version "0")
        endif()
    endif()

    set(existing "")
    foreach(language IN ITEMS C CXX)
        if(DEFINED CMAKE_${language}_COMPILER_LAUNCHER)
            string(APPEND existing "${CMAKE_${language}_COMPILER_LAUNCHER}")
        endif()
    endforeach()
    bpr_compiler_cache_decision(
        decision
        MODE "${BPR_COMPILER_CACHE}"
        PROGRAM "${program}"
        VERSION "${version}"
        DISTRIBUTION "${BPR_DISTRIBUTION_BUILD}"
        GENERATOR "${CMAKE_GENERATOR}"
        C_COMPILER_ID "${CMAKE_C_COMPILER_ID}"
        CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}"
        BUILD_TYPE "${CMAKE_BUILD_TYPE}"
        EXISTING_LAUNCHER "${existing}"
    )
    set(BPR_COMPILER_CACHE_STATE "${decision}" CACHE INTERNAL "What configure decided for BPR_COMPILER_CACHE")
    if(decision MATCHES "^error: (.*)$")
        message(FATAL_ERROR "${CMAKE_MATCH_1}")
    endif()
    if(NOT decision STREQUAL "enabled")
        set(BPR_COMPILER_CACHE_LAUNCHER "" CACHE INTERNAL "The configured ccache launcher")
        message(STATUS "Compiler cache: off (${decision})")
        return()
    endif()

    set(clone "")
    if(APPLE)
        set(clone APPLE_CLONE)
    endif()
    bpr_compiler_cache_launcher(
        launcher
        PROGRAM "${program}"
        CONFIG "${PROJECT_SOURCE_DIR}/cmake/bpr-ccache.conf"
        CACHE_DIR "${BPR_COMPILER_CACHE_DIR}"
        BASE_DIR "${PROJECT_SOURCE_DIR}"
        ${clone}
    )
    bpr_compiler_cache_write_directory_config("${BPR_COMPILER_CACHE_DIR}" "${launcher}")
    set(BPR_COMPILER_CACHE_LAUNCHER "${launcher}" CACHE INTERNAL "The configured ccache launcher")
    set(CMAKE_C_COMPILER_LAUNCHER "${launcher}" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${launcher}" PARENT_SCOPE)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-fdebug-prefix-map=${PROJECT_BINARY_DIR}=.>")
    message(STATUS "Compiler cache: ccache ${version} at ${BPR_COMPILER_CACHE_DIR}")
endfunction()
