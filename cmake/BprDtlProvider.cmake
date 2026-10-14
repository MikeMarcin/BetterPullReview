include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1481]]. dtl for the diff engine's intraline ranges, one of the
# owner-approved pure core libraries (RFC-BPR-0082 Q2, 2026-10-13), approved
# for per-line intraline comparison only. It is header-only: only the verified
# archive's `dtl/` headers are put on the include path. No upstream build,
# tests or examples run, and the core calls only `dtl::Diff` over one line
# pair's Unicode scalars, never over a file's lines.
function(bpr_acquire_dtl)
    if(TARGET BetterPullReview::Dtl OR TARGET better_pull_review_dtl)
        message(FATAL_ERROR "dtl target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRDTL)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRDTL}" STREQUAL "")
            message(FATAL_ERROR "dtl source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/dtl-1.21.tar.gz")
    set(digest "90ed2dbf4e6d687737fe25f118bbcb6aed778cecc3f2115d191a032bf8643dbd")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached dtl 1.21 archive")
        endif()
        file(DOWNLOAD "https://codeload.github.com/cubicdaiya/dtl/tar.gz/refs/tags/v1.21"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 26321)
        message(FATAL_ERROR "dtl archive does not match pinned 1.21")
    endif()
    # Re-extract only the headers and the licence from the verified archive:
    # never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/dtl")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}"
         PATTERNS "dtl-1.21/dtl" "dtl-1.21/COPYING")
    set(source "${stage}/dtl-1.21")
    file(SHA256 "${source}/COPYING" license_digest)
    if(NOT license_digest STREQUAL "7331165008d2c296d7cd0fc65ed752cd169fd7e4604a6413f9a1eef8c0e14fec")
        message(FATAL_ERROR "dtl license does not match the pinned notice")
    endif()
    add_library(better_pull_review_dtl INTERFACE)
    add_library(BetterPullReview::Dtl ALIAS better_pull_review_dtl)
    target_include_directories(better_pull_review_dtl SYSTEM INTERFACE "${source}")
    target_compile_features(better_pull_review_dtl INTERFACE cxx_std_23)
endfunction()
