include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1435]]. zlib for the functional core's review-bundle reader, one of the
# owner-approved core libraries (2026-10-13). Only the raw-inflate, checksum
# and, since [[BPR-PENDING-AG]], raw-deflate sources are compiled: no gz* file
# I/O, no compress/uncompress helpers, no upstream build. The owner admitted
# deflate on 2026-10-21 for the diagnostics export's `.zip`.
# Z_PREFIX renames every symbol to z_*, so this copy can never be confused with
# the system zlib that LibArchive links on macOS.
function(bpr_acquire_zlib)
    if(TARGET BetterPullReview::ZLib OR TARGET better_pull_review_zlib)
        message(FATAL_ERROR "zlib target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRZLIB)
        if(NOT "${FETCHCONTENT_SOURCE_DIR_BPRZLIB}" STREQUAL "")
            message(FATAL_ERROR "zlib source substitution is forbidden")
        endif()
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/zlib-1.3.2.tar.gz")
    set(digest "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached zlib 1.3.2 archive")
        endif()
        file(DOWNLOAD "https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 1502830)
        message(FATAL_ERROR "zlib archive does not match pinned 1.3.2")
    endif()
    # Re-extract the verified archive: never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/zlib")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}")
    set(source "${stage}/zlib-1.3.2")
    file(SHA256 "${source}/LICENSE" license_digest)
    if(NOT license_digest STREQUAL "e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2")
        message(FATAL_ERROR "zlib license does not match the pinned notice")
    endif()
    add_library(
        better_pull_review_zlib
        STATIC
            "${source}/adler32.c"
            "${source}/crc32.c"
            "${source}/deflate.c"
            "${source}/inffast.c"
            "${source}/inflate.c"
            "${source}/inftrees.c"
            "${source}/trees.c"
            "${source}/zutil.c"
    )
    add_library(BetterPullReview::ZLib ALIAS better_pull_review_zlib)
    target_include_directories(better_pull_review_zlib SYSTEM PUBLIC "${source}")
    target_compile_definitions(better_pull_review_zlib PUBLIC Z_PREFIX ZLIB_CONST)
    if(MSVC)
        target_compile_definitions(better_pull_review_zlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
    endif()
    set_target_properties(better_pull_review_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
