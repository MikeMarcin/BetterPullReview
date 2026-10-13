include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1446]]. RFC-BPR-0080's evaluation-only hash candidates. Nothing here is
# part of the product: only tests/hash_evaluation includes this file, and only
# when BPR_HASH_EVALUATION is ON. Core links none of these targets, and none of
# them is in core's link allow-list (cmake/VerifyCoreBoundary.cmake) until the
# owner answers the RFC's Q1 and Q2.
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

# Defines better_pull_review_eval_sha1dc and better_pull_review_eval_hacl_sha2,
# and sets aVectors to the directory of the NIST CAVP byte-oriented SHA vectors
# and aCollisions to sha1dc's collision test files.
function(bpr_acquire_hash_evaluation aVectors aCollisions)
    if(TARGET better_pull_review_eval_sha1dc OR TARGET better_pull_review_eval_hacl_sha2)
        message(FATAL_ERROR "Hash evaluation targets must be owned by the pinned provider")
    endif()

    # sha1collisiondetection master, 2026-10-13: Git's and GitHub's SHA-1.
    set(sha1dc_revision "a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988")
    bpr_hash_evaluation_archive(
        sha1dc
        "https://codeload.github.com/cr-marcstevens/sha1collisiondetection/tar.gz/${sha1dc_revision}"
        "sha1collisiondetection-${sha1dc_revision}.tar.gz"
        "b9c89e19a1b02cb623d81515b5a281781a8c3f2296a2548854f5e1455b1b1729"
        787039
        sha1dc_stage
    )
    set(sha1dc "${sha1dc_stage}/sha1collisiondetection-${sha1dc_revision}")
    bpr_hash_evaluation_require_digest("${sha1dc}/LICENSE.txt" "9daac99c665ddc38fde6d7c91b9173c112d5e6c6e7e5f7d8bb8747c630efc83d")
    add_library(better_pull_review_eval_sha1dc STATIC "${sha1dc}/lib/sha1.c" "${sha1dc}/lib/ubc_check.c")
    target_include_directories(better_pull_review_eval_sha1dc SYSTEM PUBLIC "${sha1dc}/lib")
    target_include_directories(better_pull_review_eval_sha1dc PRIVATE "${PROJECT_SOURCE_DIR}/tests/hash_evaluation")
    # Configured as Git configures it: plain SHA-1 output (no safe-hash
    # substitution), detection and unavoidable-bit-condition checks on, and
    # no standard headers beyond the ones named in sha1dc_includes.h.
    target_compile_definitions(
        better_pull_review_eval_sha1dc
        PRIVATE
            SHA1DC_NO_STANDARD_INCLUDES
            SHA1DC_INIT_SAFE_HASH_DEFAULT=0
            "SHA1DC_CUSTOM_INCLUDE_SHA1_C=\"sha1dc_includes.h\""
            "SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C=\"sha1dc_includes.h\""
    )

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

    foreach(target IN ITEMS better_pull_review_eval_sha1dc better_pull_review_eval_hacl_sha2)
        set_target_properties("${target}" PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)
        # The core is built hardened, so the candidates are measured the same
        # way. MSVC's /sdl is left to the Windows station to try.
        if(NOT MSVC)
            bpr_enable_hardening("${target}")
        endif()
    endforeach()
    set(${aVectors} "${cavp_stage}/shabytetestvectors" PARENT_SCOPE)
    set(${aCollisions} "${sha1dc}/test" PARENT_SCOPE)
endfunction()
