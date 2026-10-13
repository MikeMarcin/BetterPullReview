include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/BprRe2Provider.cmake")

# [[BPR-1458]]. Evaluation only: candidate L of RFC-BPR-0083. libgit2 1.9.7,
# acquired through the same verified-archive path as RE2 (pinned SHA-256, byte
# size and COPYING digest, re-extracted under build/_bpr) and built static with
# no HTTPS, SSH, NTLM, GSSAPI, CLI, tests, examples or fuzzers, its bundled
# PCRE2 as the regex backend, the system zlib, and nothing installed. Only
# tests/evaluation/libgit2 includes this file, and only when
# BPR_EVALUATE_LIBGIT2 is on; the product never links it.
set(BPR_LIBGIT2_VERSION "1.9.7")

function(bpr_acquire_libgit2)
    if(TARGET libgit2package OR TARGET BetterPullReview::Libgit2Evaluation)
        message(FATAL_ERROR "libgit2 targets must be owned by the pinned evaluation provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRLIBGIT2)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRLIBGIT2}" STREQUAL "")
            message(FATAL_ERROR "libgit2 source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()

    set(stage "${CMAKE_BINARY_DIR}/_bpr/libgit2")
    set(source "${stage}/libgit2-${BPR_LIBGIT2_VERSION}")
    bpr_fetch_verified_archive(
        "libgit2 ${BPR_LIBGIT2_VERSION}"
        "https://codeload.github.com/libgit2/libgit2/tar.gz/refs/tags/v${BPR_LIBGIT2_VERSION}"
        "${CPM_SOURCE_CACHE}/libgit2-${BPR_LIBGIT2_VERSION}.tar.gz"
        "1a4fbe7589e814777ae76b64734ad80f4ecad22cd33a22682a2aaea4ae5375e7"
        7769162
        "${source}/COPYING"
        "e0938121c0554985fe17196ac38fc590d149268e9c46e0c0e7a2eae1354fae71"
        "${stage}")

    # libgit2 reads these as ordinary options; set them for this graph only.
    # EXPERIMENTAL_SHA256 follows BPR_EVALUATE_LIBGIT2_SHA256 so the
    # experimental variant can be measured separately (RFC-BPR-0083 H3).
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTS OFF)
    set(BUILD_CLI OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_FUZZERS OFF)
    set(USE_HTTPS OFF)
    set(USE_SSH OFF)
    set(USE_NTLMCLIENT OFF)
    set(USE_GSSAPI OFF)
    set(USE_HTTP_PARSER builtin)
    set(USE_SHA1 CollisionDetection)
    set(USE_SHA256 builtin)
    set(REGEX_BACKEND builtin)
    # libgit2's bundled zlib defines the same internal symbols (z_errmsg) as
    # core's pinned zlib, so a program linking both fails with a duplicate
    # symbol. The evaluation build uses the system zlib instead.
    set(USE_BUNDLED_ZLIB OFF)
    set(USE_ICONV OFF)
    set(SONAME OFF)
    set(EXPERIMENTAL_SHA256 ${BPR_EVALUATE_LIBGIT2_SHA256})
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    # libgit2 declares its backend choices with set(CACHE); under its 3.5 policy
    # floor that set would discard the values above.
    set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
    # libgit2 1.9.7 declares cmake_minimum_required(3.5.1), which CMake 4
    # reports as deprecated; the presets make deprecation an error. Raise the
    # policy floor for this subproject only instead of weakening the presets.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

    # libgit2 caches CMAKE_C_STANDARD (90) and CMAKE_C_EXTENSIONS for the whole
    # build. Its own targets keep the values they were created with; drop the
    # cache entries afterwards so no product C target inherits them.
    set(c_standard_cached FALSE)
    if(DEFINED CACHE{CMAKE_C_STANDARD})
        set(c_standard_cached TRUE)
    endif()
    set(c_extensions_cached FALSE)
    if(DEFINED CACHE{CMAKE_C_EXTENSIONS})
        set(c_extensions_cached TRUE)
    endif()

    add_subdirectory("${source}" "${CMAKE_BINARY_DIR}/_bpr/libgit2-build" EXCLUDE_FROM_ALL SYSTEM)

    if(NOT c_standard_cached)
        unset(CMAKE_C_STANDARD CACHE)
    endif()
    if(NOT c_extensions_cached)
        unset(CMAKE_C_EXTENSIONS CACHE)
    endif()

    if(NOT TARGET libgit2package)
        message(FATAL_ERROR "libgit2 ${BPR_LIBGIT2_VERSION} did not provide libgit2package")
    endif()
    add_library(better_pull_review_libgit2_evaluation INTERFACE)
    add_library(BetterPullReview::Libgit2Evaluation ALIAS better_pull_review_libgit2_evaluation)
    # The bundled experimental.h enables nothing; the SHA-256 variant's API is
    # selected by the same definition libgit2's own sources compile with.
    target_include_directories(better_pull_review_libgit2_evaluation SYSTEM INTERFACE "${source}/include")
    if(BPR_EVALUATE_LIBGIT2_SHA256)
        target_compile_definitions(better_pull_review_libgit2_evaluation INTERFACE GIT_EXPERIMENTAL_SHA256=1)
    endif()
    target_link_libraries(better_pull_review_libgit2_evaluation INTERFACE libgit2package)
    set(BPR_LIBGIT2_SOURCE_DIR "${source}" PARENT_SCOPE)
endfunction()
