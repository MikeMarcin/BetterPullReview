include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1485]]. Git 2.56.0's xdiff, the diff engine's primary line matcher
# (RFC-BPR-0082: `l-myers-indent`, Myers with the indent heuristic). The owner
# accepted LGPL-2.1-or-later xdiff only "if xdiff is dynamically linked as its
# own shared library, as Qt is" (Q1, 2026-10-13), so this is a SHARED library
# and nothing else: no static or object copy exists, and only the xdiff adapter
# (src/adapters/xdiff) links it; `architecture_boundaries` checks both. Only the
# verified archive's xdiff/ sources, LGPL-2.1 notice and COPYING are extracted;
# third_party/git-xdiff/git-compat-util.h replaces Git's own compatibility
# header for this build. xhistogram.c (EDL-1.0) is compiled because xdiffi.c
# links to it; the product never selects histogram.
function(bpr_acquire_xdiff)
    if(TARGET BetterPullReview::Xdiff OR TARGET better_pull_review_xdiff)
        message(FATAL_ERROR "xdiff target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRXDIFF)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRXDIFF}" STREQUAL "")
            message(FATAL_ERROR "xdiff source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/git-2.56.0.tar.xz")
    set(digest "26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached Git 2.56.0 archive")
        endif()
        file(DOWNLOAD "https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.56.0.tar.xz"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 8489656)
        message(FATAL_ERROR "Git archive does not match pinned 2.56.0")
    endif()
    # Re-extract only xdiff/ and the notices from the verified archive: never
    # trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/xdiff")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}"
         PATTERNS "git-2.56.0/xdiff" "git-2.56.0/LGPL-2.1" "git-2.56.0/COPYING")
    set(source "${stage}/git-2.56.0")
    file(SHA256 "${source}/LGPL-2.1" lgpl_digest)
    file(SHA256 "${PROJECT_SOURCE_DIR}/third_party/git-xdiff/LGPL-2.1" retained_lgpl_digest)
    if(NOT lgpl_digest STREQUAL "1922f45d2c49e390032c9c0ba6d7cac904087f7cec51af30c2b2ad022ce0e76a"
       OR NOT retained_lgpl_digest STREQUAL lgpl_digest)
        message(FATAL_ERROR "Git 2.56.0 LGPL-2.1 notice does not match the pinned notice")
    endif()
    file(SHA256 "${source}/COPYING" copying_digest)
    if(NOT copying_digest STREQUAL "5b2198d1645f767585e8a88ac0499b04472164c0d2da22e75ecf97ef443ab32e")
        message(FATAL_ERROR "Git 2.56.0 COPYING does not match the pinned notice")
    endif()
    set(sources
        "${source}/xdiff/xdiffi.c"
        "${source}/xdiff/xemit.c"
        "${source}/xdiff/xhistogram.c"
        "${source}/xdiff/xmerge.c"
        "${source}/xdiff/xpatience.c"
        "${source}/xdiff/xprepare.c"
        "${source}/xdiff/xutils.c")
    add_library(better_pull_review_xdiff SHARED ${sources})
    add_library(BetterPullReview::Xdiff ALIAS better_pull_review_xdiff)
    # Only xdiff/ and the shim are on an include path: Git's top level holds a
    # file named `version`, which a case-insensitive file system would find for
    # the standard <version> header.
    target_include_directories(better_pull_review_xdiff PRIVATE "${PROJECT_SOURCE_DIR}/third_party/git-xdiff")
    target_include_directories(better_pull_review_xdiff SYSTEM PUBLIC
        "${source}/xdiff" "${PROJECT_SOURCE_DIR}/third_party/git-xdiff")
    if(MSVC)
        target_compile_options(better_pull_review_xdiff PRIVATE /w)
    else()
        target_compile_options(better_pull_review_xdiff PRIVATE -w)
    endif()
    set_target_properties(better_pull_review_xdiff PROPERTIES
        OUTPUT_NAME bpr_xdiff
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET default
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
endfunction()
