include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")

# [[BPR-1431]]. Source search's regex engine: pinned Google RE2 and the
# Abseil LTS it is released against, both static. Archives are verified by
# SHA-256 and size and re-extracted into the build tree, never trusted as an
# editable cached source. Only the libraries are built: no tests, benchmarks,
# install rules, ICU or PCRE.
function(bpr_fetch_verified_archive aName aUrl aArchive aDigest aBytes aLicense aLicenseDigest aStage)
    file(LOCK "${aArchive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${aArchive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached ${aName} archive ${aArchive}")
        endif()
        file(DOWNLOAD "${aUrl}" "${aArchive}" EXPECTED_HASH "SHA256=${aDigest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${aArchive}" actual)
    file(SIZE "${aArchive}" bytes)
    if(NOT actual STREQUAL aDigest OR NOT bytes EQUAL aBytes)
        message(FATAL_ERROR "${aName} archive does not match its pinned digest and size")
    endif()
    file(REMOVE_RECURSE "${aStage}")
    file(MAKE_DIRECTORY "${aStage}")
    file(ARCHIVE_EXTRACT INPUT "${aArchive}" DESTINATION "${aStage}")
    file(SHA256 "${aLicense}" license)
    if(NOT license STREQUAL aLicenseDigest)
        message(FATAL_ERROR "${aName} license does not match the pinned notice")
    endif()
endfunction()

function(bpr_acquire_re2)
    if(TARGET re2::re2 OR TARGET absl::base OR TARGET BetterPullReview::RE2)
        message(FATAL_ERROR "RE2 and Abseil targets must be owned by the pinned provider")
    endif()
    foreach(substitution IN ITEMS FETCHCONTENT_SOURCE_DIR_BPRRE2 FETCHCONTENT_SOURCE_DIR_BPRABSEIL)
        if(DEFINED ${substitution})
            if(NOT "${${substitution}}" STREQUAL "")
                message(FATAL_ERROR "RE2/Abseil source substitution is forbidden")
            endif()
        endif()
    endforeach()
    bpr_load_cpm()

    set(stage "${CMAKE_BINARY_DIR}/_bpr/re2")
    bpr_fetch_verified_archive(
        "Abseil 20250814.2"
        "https://codeload.github.com/abseil/abseil-cpp/tar.gz/refs/tags/20250814.2"
        "${CPM_SOURCE_CACHE}/abseil-cpp-20250814.2.tar.gz"
        "f9148fb00ec98a2396bdf875c99a78e6a70afa662b107862d92b285d857a8320"
        2235771
        "${stage}/abseil/abseil-cpp-20250814.2/LICENSE"
        "c79a7fea0e3cac04cd43f20e7b648e5a0ff8fa5344e644b0ee09ca1162b62747"
        "${stage}/abseil")
    bpr_fetch_verified_archive(
        "RE2 2025-11-05"
        "https://codeload.github.com/google/re2/tar.gz/refs/tags/2025-11-05"
        "${CPM_SOURCE_CACHE}/re2-2025-11-05.tar.gz"
        "87f6029d2f6de8aa023654240a03ada90e876ce9a4676e258dd01ea4c26ffd67"
        397504
        "${stage}/re2/re2-2025-11-05/LICENSE"
        "6040cda75d90b1738292a631d89934c411ef7ffd543c4d6a1b7edfc8edf29449"
        "${stage}/re2")

    # Abseil and RE2 read these as ordinary cache options; set them for this
    # graph only so the product's own BUILD_TESTING and install policy stay.
    # Abseil requires every library using it to share one language standard;
    # the product builds as C++23, so both libraries do too.
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(BUILD_SHARED_LIBS OFF)
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_ENABLE_INSTALL OFF)
    set(ABSL_BUILD_TESTING OFF)
    set(ABSL_BUILD_TEST_HELPERS OFF)
    set(ABSL_USE_EXTERNAL_GOOGLETEST OFF)
    set(RE2_BUILD_TESTING OFF)
    set(RE2_TEST OFF)
    set(RE2_BENCHMARK OFF)
    set(RE2_INSTALL OFF)
    set(RE2_USE_ICU OFF)
    set(RE2_BUILD_FRAMEWORK OFF)
    set(USEPCRE OFF)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    add_subdirectory("${stage}/abseil/abseil-cpp-20250814.2" "${CMAKE_BINARY_DIR}/_bpr/re2-build/abseil" EXCLUDE_FROM_ALL SYSTEM)
    add_subdirectory("${stage}/re2/re2-2025-11-05" "${CMAKE_BINARY_DIR}/_bpr/re2-build/re2" EXCLUDE_FROM_ALL SYSTEM)

    add_library(better_pull_review_re2 INTERFACE)
    add_library(BetterPullReview::RE2 ALIAS better_pull_review_re2)
    target_link_libraries(better_pull_review_re2 INTERFACE re2::re2)

    # Release notices for both components, kept beside the product build.
    file(READ "${stage}/re2/re2-2025-11-05/LICENSE" re2_notice)
    file(READ "${stage}/abseil/abseil-cpp-20250814.2/LICENSE" abseil_notice)
    file(WRITE "${CMAKE_BINARY_DIR}/_bpr/RE2-NOTICES.txt"
         "RE2 2025-11-05\n\n${re2_notice}\n\nAbseil 20250814.2\n\n${abseil_notice}")
endfunction()
