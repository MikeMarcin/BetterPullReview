include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Hardening.cmake")

# [[BPR-1451]]. HACL*'s generated, formally verified SHA-2 C for the core's
# SHA-256. It is one of the owner-approved core libraries (RFC-BPR-0080
# Q2, 2026-10-13). Only the generated distribution's Hacl_Hash_SHA2.c is
# compiled, from dist/gcc-compatible (dist/msvc-compatible under MSVC), the
# flavour HACL* generates for the compiler. No F* toolchain, upstream build or
# tests run, and the core calls only the block-level SHA-256 functions, never
# the allocating streaming API.
function(bpr_acquire_hacl_sha2)
    if(TARGET BetterPullReview::HaclSha2 OR TARGET better_pull_review_hacl_sha2)
        message(FATAL_ERROR "HACL* SHA-2 target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRHACLSHA2)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRHACLSHA2}" STREQUAL "")
            message(FATAL_ERROR "HACL* source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(revision "504c2987452f87fe44bce9b9f12e19d6e051761f")
    set(archive "${CPM_SOURCE_CACHE}/hacl-star-${revision}.tar.gz")
    set(digest "e32f7add802f44e12c3f347824ddfd074abfe5cd5ccb291a942786a8c36af52c")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached HACL* ${revision} archive")
        endif()
        file(DOWNLOAD "https://codeload.github.com/hacl-star/hacl-star/tar.gz/${revision}"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 10228408)
        message(FATAL_ERROR "HACL* archive does not match pinned ${revision}")
    endif()
    # Re-extract only the generated distribution from the verified archive:
    # never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/hacl-sha2")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}" PATTERNS "hacl-star-${revision}/dist")
    set(dist "${stage}/hacl-star-${revision}/dist")
    # The generated C carries, and is distributed under, dist/LICENSE.txt
    # (MIT); the repository's top-level Apache-2.0 LICENSE covers the F*
    # sources, which are not compiled.
    file(SHA256 "${dist}/LICENSE.txt" license_digest)
    if(NOT license_digest STREQUAL "998ce04fb8ad9dedb0bc1b44938f8c3dcf1089780fa105ce1c8c30fb5554d78c")
        message(FATAL_ERROR "HACL* license does not match the pinned notice")
    endif()
    set(flavour gcc-compatible)
    if(MSVC)
        set(flavour msvc-compatible)
    endif()
    add_library(better_pull_review_hacl_sha2 STATIC "${dist}/${flavour}/Hacl_Hash_SHA2.c")
    add_library(BetterPullReview::HaclSha2 ALIAS better_pull_review_hacl_sha2)
    target_include_directories(
        better_pull_review_hacl_sha2
        SYSTEM PUBLIC
            "${dist}/${flavour}"
            "${dist}/karamel/include"
            "${dist}/karamel/krmllib/dist/minimal"
    )
    set_target_properties(better_pull_review_hacl_sha2 PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # [[BPR-1630]]. Karamel's Linux endian.h branch uses the libc endian
        # helpers hidden by glibc under strict C11 without this feature set.
        # Keep their admission local to generated C, not its core consumers.
        target_compile_definitions(better_pull_review_hacl_sha2 PRIVATE _DEFAULT_SOURCE)
    endif()
    # Built hardened like the core that links it.
    bpr_enable_hardening(better_pull_review_hacl_sha2)
endfunction()
