include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Hardening.cmake")

# [[BPR-1450]]. sha1collisiondetection (sha1dc), the SHA-1 Git and GitHub use,
# for every SHA-1 Git object identity the functional core computes. It is one
# of the owner-approved core libraries (RFC-BPR-0080 Q1, 2026-10-13).
# Only lib/sha1.c and lib/ubc_check.c are compiled, configured as Git
# configures them: plain SHA-1 output (no safe-hash substitution), collision
# detection and unavoidable-bit-condition checks on, and no standard headers
# beyond third_party/sha1collisiondetection/sha1dc_includes.h. No upstream
# build, tests or command-line tool run.
function(bpr_acquire_sha1dc)
    if(TARGET BetterPullReview::Sha1dc OR TARGET better_pull_review_sha1dc)
        message(FATAL_ERROR "sha1collisiondetection target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRSHA1DC)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRSHA1DC}" STREQUAL "")
            message(FATAL_ERROR "sha1collisiondetection source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(revision "a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988")
    set(archive "${CPM_SOURCE_CACHE}/sha1collisiondetection-${revision}.tar.gz")
    set(digest "b9c89e19a1b02cb623d81515b5a281781a8c3f2296a2548854f5e1455b1b1729")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached sha1collisiondetection ${revision} archive")
        endif()
        file(DOWNLOAD "https://codeload.github.com/cr-marcstevens/sha1collisiondetection/tar.gz/${revision}"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 787039)
        message(FATAL_ERROR "sha1collisiondetection archive does not match pinned ${revision}")
    endif()
    # Re-extract the verified archive: never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/sha1dc")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}")
    set(source "${stage}/sha1collisiondetection-${revision}")
    file(SHA256 "${source}/LICENSE.txt" license_digest)
    if(NOT license_digest STREQUAL "9daac99c665ddc38fde6d7c91b9173c112d5e6c6e7e5f7d8bb8747c630efc83d")
        message(FATAL_ERROR "sha1collisiondetection license does not match the pinned notice")
    endif()
    add_library(better_pull_review_sha1dc STATIC "${source}/lib/sha1.c" "${source}/lib/ubc_check.c")
    add_library(BetterPullReview::Sha1dc ALIAS better_pull_review_sha1dc)
    target_include_directories(better_pull_review_sha1dc SYSTEM PUBLIC "${source}/lib")
    target_include_directories(better_pull_review_sha1dc PRIVATE "${PROJECT_SOURCE_DIR}/third_party/sha1collisiondetection")
    target_compile_definitions(
        better_pull_review_sha1dc
        PRIVATE
            SHA1DC_NO_STANDARD_INCLUDES
            SHA1DC_INIT_SAFE_HASH_DEFAULT=0
            "SHA1DC_CUSTOM_INCLUDE_SHA1_C=\"sha1dc_includes.h\""
            "SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C=\"sha1dc_includes.h\""
    )
    set_target_properties(better_pull_review_sha1dc PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)
    # Built hardened like the core that links it; upstream's own MSVC project
    # also builds with /sdl and warnings as errors.
    bpr_enable_hardening(better_pull_review_sha1dc)
    # Upstream's collision test files (SHAttered, SHA-mbles and the
    # reduced-round vector), which the hash evaluation reads.
    set(BPR_SHA1DC_TEST_DIRECTORY "${source}/test" CACHE INTERNAL "sha1collisiondetection's own collision test files")
endfunction()
