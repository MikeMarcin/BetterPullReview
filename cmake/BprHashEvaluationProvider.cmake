include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1446]]. RFC-BPR-0080's evaluation-only hash candidates. Nothing here is
# part of the product: only tests/hash_evaluation includes this file, and only
# when BPR_HASH_EVALUATION is ON. Core links none of these targets.
#
# Each archive is pinned by SHA-256 and size, verified before use, and
# extracted again from the verified archive into the build tree, so an edited
# cached source tree is never trusted (the zlib provider's pattern).

function(bpr_hash_evaluation_archive aName aUrl aFile aDigest aBytes aOutput)
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/${aFile}")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires the cached ${aName} archive ${aFile}")
        endif()
        file(DOWNLOAD "${aUrl}" "${archive}" EXPECTED_HASH "SHA256=${aDigest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL aDigest OR NOT bytes EQUAL aBytes)
        message(FATAL_ERROR "The ${aName} archive does not match its pin")
    endif()
    set(stage "${CMAKE_BINARY_DIR}/_bpr/hash-evaluation/${aName}")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}")
    set(${aOutput} "${stage}" PARENT_SCOPE)
endfunction()

function(bpr_hash_evaluation_require_digest aFile aDigest)
    file(SHA256 "${aFile}" actual)
    if(NOT actual STREQUAL aDigest)
        message(FATAL_ERROR "${aFile} does not match its pinned digest")
    endif()
endfunction()

# Defines better_pull_review_eval_hacl_sha2, and sets aVectors to the
# directory of the NIST CAVP byte-oriented SHA vectors and aCollisions to
# sha1dc's collision test files. sha1collisiondetection itself is the
# product's BetterPullReview::Sha1dc ([[BPR-1450]]): the core links it, so the
# evaluation measures that same target instead of building a second copy.
function(bpr_acquire_hash_evaluation aVectors aCollisions)
    if(TARGET better_pull_review_eval_hacl_sha2)
        message(FATAL_ERROR "Hash evaluation targets must be owned by the pinned provider")
    endif()
    if(NOT TARGET BetterPullReview::Sha1dc OR NOT BPR_SHA1DC_TEST_DIRECTORY)
        message(FATAL_ERROR "The hash evaluation needs the product's sha1collisiondetection provider")
    endif()

    # HACL* main, 2026-10-13: the generated portable C, in the distribution
    # HACL* generates for the compiler (dist/msvc-compatible for MSVC).
    set(hacl_revision "504c2987452f87fe44bce9b9f12e19d6e051761f")
    bpr_hash_evaluation_archive(
        hacl
        "https://codeload.github.com/hacl-star/hacl-star/tar.gz/${hacl_revision}"
        "hacl-star-${hacl_revision}.tar.gz"
        "e32f7add802f44e12c3f347824ddfd074abfe5cd5ccb291a942786a8c36af52c"
        10228408
        hacl_stage
    )
    set(hacl "${hacl_stage}/hacl-star-${hacl_revision}/dist")
    bpr_hash_evaluation_require_digest("${hacl}/LICENSE.txt" "998ce04fb8ad9dedb0bc1b44938f8c3dcf1089780fa105ce1c8c30fb5554d78c")
    set(hacl_flavour gcc-compatible)
    if(MSVC)
        set(hacl_flavour msvc-compatible)
    endif()
    add_library(better_pull_review_eval_hacl_sha2 STATIC "${hacl}/${hacl_flavour}/Hacl_Hash_SHA2.c")
    target_include_directories(
        better_pull_review_eval_hacl_sha2
        SYSTEM PUBLIC
            "${hacl}/${hacl_flavour}"
            "${hacl}/karamel/include"
            "${hacl}/karamel/krmllib/dist/minimal"
    )

    # NIST CAVP SHA byte-oriented test vectors (SHAVS), the RFC's known answers.
    bpr_hash_evaluation_archive(
        cavp
        "https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/shs/shabytetestvectors.zip"
        "nist-cavp-shabytetestvectors.zip"
        "929ef80b7b3418aca026643f6f248815913b60e01741a44bba9e118067f4c9b8"
        4909729
        cavp_stage
    )

    set_target_properties(better_pull_review_eval_hacl_sha2 PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)
    # The core is built hardened, so the candidate is measured the same way.
    # MSVC's /sdl is left to the Windows station to try.
    if(NOT MSVC)
        bpr_enable_hardening(better_pull_review_eval_hacl_sha2)
    endif()
    set(${aVectors} "${cavp_stage}/shabytetestvectors" PARENT_SCOPE)
    set(${aCollisions} "${BPR_SHA1DC_TEST_DIRECTORY}" PARENT_SCOPE)
endfunction()
