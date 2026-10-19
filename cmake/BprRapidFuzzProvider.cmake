include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1480]]. rapidfuzz-cpp for the diff engine's line-pair similarity, one
# of the owner-approved core libraries (RFC-BPR-0082 Q2, 2026-10-13). It
# is header-only: only the verified archive's `rapidfuzz/` headers are put on
# the include path. No upstream build, tests, benchmarks, fuzzers or install
# rules run, and the core calls only `rapidfuzz::CachedIndel`.
function(bpr_acquire_rapidfuzz)
    if(TARGET BetterPullReview::RapidFuzz OR TARGET better_pull_review_rapidfuzz)
        message(FATAL_ERROR "rapidfuzz-cpp target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRRAPIDFUZZ)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRRAPIDFUZZ}" STREQUAL "")
            message(FATAL_ERROR "rapidfuzz-cpp source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/rapidfuzz-cpp-3.3.4.tar.gz")
    set(digest "a0dd2ef361cac165e12076696e7c7e8d069a2908abd9599ad4bd190de33f9881")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached rapidfuzz-cpp 3.3.4 archive")
        endif()
        file(DOWNLOAD "https://codeload.github.com/rapidfuzz/rapidfuzz-cpp/tar.gz/refs/tags/v3.3.4"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 314599)
        message(FATAL_ERROR "rapidfuzz-cpp archive does not match pinned 3.3.4")
    endif()
    # Re-extract only the headers and the licence from the verified archive:
    # never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/rapidfuzz")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}"
         PATTERNS "rapidfuzz-cpp-3.3.4/rapidfuzz" "rapidfuzz-cpp-3.3.4/LICENSE")
    set(source "${stage}/rapidfuzz-cpp-3.3.4")
    file(SHA256 "${source}/LICENSE" license_digest)
    if(NOT license_digest STREQUAL "529ac8f069bd6aa633a2da2bea8de4299f43dec428aa407585f2b460dd98944f")
        message(FATAL_ERROR "rapidfuzz-cpp license does not match the pinned notice")
    endif()
    add_library(better_pull_review_rapidfuzz INTERFACE)
    add_library(BetterPullReview::RapidFuzz ALIAS better_pull_review_rapidfuzz)
    target_include_directories(better_pull_review_rapidfuzz SYSTEM INTERFACE "${source}")
    target_compile_features(better_pull_review_rapidfuzz INTERFACE cxx_std_23)
endfunction()
