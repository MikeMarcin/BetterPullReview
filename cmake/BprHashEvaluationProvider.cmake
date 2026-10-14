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

# Sets aVectors to the directory of the NIST CAVP byte-oriented SHA vectors
# and aCollisions to sha1dc's collision test files. Both candidate libraries
# are now the product's own targets: sha1collisiondetection is
# BetterPullReview::Sha1dc ([[BPR-1450]]) and HACL*'s SHA-2 is
# BetterPullReview::HaclSha2 ([[BPR-1451]]). The core links them, so the
# evaluation measures those same targets instead of building second copies,
# which would also define their symbols twice.
function(bpr_acquire_hash_evaluation aVectors aCollisions)
    if(NOT TARGET BetterPullReview::Sha1dc OR NOT BPR_SHA1DC_TEST_DIRECTORY)
        message(FATAL_ERROR "The hash evaluation needs the product's sha1collisiondetection provider")
    endif()
    if(NOT TARGET BetterPullReview::HaclSha2)
        message(FATAL_ERROR "The hash evaluation needs the product's HACL* SHA-2 provider")
    endif()

    # NIST CAVP SHA byte-oriented test vectors (SHAVS), the RFC's known answers.
    bpr_hash_evaluation_archive(
        cavp
        "https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/shs/shabytetestvectors.zip"
        "nist-cavp-shabytetestvectors.zip"
        "929ef80b7b3418aca026643f6f248815913b60e01741a44bba9e118067f4c9b8"
        4909729
        cavp_stage
    )

    set(${aVectors} "${cavp_stage}/shabytetestvectors" PARENT_SCOPE)
    set(${aCollisions} "${BPR_SHA1DC_TEST_DIRECTORY}" PARENT_SCOPE)
endfunction()
