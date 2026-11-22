include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/AitoCpm.cmake")
function(bpr_acquire_quill)
    if(TARGET quill::quill OR TARGET better_pull_review_quill)
        message(FATAL_ERROR "Quill target must be owned by the pinned provider")
    endif()
    if(DEFINED FETCHCONTENT_SOURCE_DIR_BPRQUILL AND NOT FETCHCONTENT_SOURCE_DIR_BPRQUILL STREQUAL "")
        message(FATAL_ERROR "Quill source substitution is forbidden")
    endif()
    bpr_load_cpm()
    set(archive "${CPM_SOURCE_CACHE}/quill-v13.0.0.tar.gz")
    set(digest "88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b")
    file(LOCK "${archive}.lock" GUARD FUNCTION TIMEOUT 120)
    if(NOT EXISTS "${archive}")
        if(BPR_DEPENDENCY_OFFLINE)
            message(FATAL_ERROR "Offline mode requires cached Quill v13.0.0 archive")
        endif()
        file(DOWNLOAD "https://codeload.github.com/odygrd/quill/tar.gz/refs/tags/v13.0.0"
             "${archive}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
    endif()
    file(SHA256 "${archive}" actual)
    file(SIZE "${archive}" bytes)
    if(NOT actual STREQUAL digest OR NOT bytes EQUAL 2061083)
        message(FATAL_ERROR "Quill archive does not match pinned v13.0.0")
    endif()
    # Re-extract the verified archive: never trust an editable cached source tree.
    set(stage "${CMAKE_BINARY_DIR}/_bpr/quill")
    file(REMOVE_RECURSE "${stage}")
    file(MAKE_DIRECTORY "${stage}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${stage}")
    # Backport Quill d15b208c8e83f7f25eca59fea7c8b909e8d34174 from v13.1.0.
    # The backend can retire a context as soon as it observes invalidity.
    set(context_header "${stage}/quill-13.0.0/include/quill/core/ThreadContextManager.h")
    file(SHA256 "${context_header}" context_digest)
    if(NOT context_digest STREQUAL "cdb1e20c51b5002794202dda81740efbc9b4b8f58418bd63ed5bc866f5961a08")
        message(FATAL_ERROR "Quill thread context header does not match the backport input")
    endif()
    file(READ "${context_header}" context_source)
    string(REPLACE
        [=[    _thread_context->mark_invalid();

    // Notify the backend thread that one context has been removed
    ThreadContextManager::instance().add_invalid_thread_context();
]=]
        [=[    // Count retirement before publishing invalidity: the backend may remove it immediately.
    ThreadContextManager::instance().add_invalid_thread_context();
    _thread_context->mark_invalid();
]=]
        patched_context_source "${context_source}")
    string(SHA256 patched_context_digest "${patched_context_source}")
    if(NOT patched_context_digest STREQUAL "4688eb89dcb4ec3bcc66320ab461b116c7d74f7fb6ea589697a148b9f048ec44")
        message(FATAL_ERROR "Quill thread context backport did not produce the expected header")
    endif()
    file(WRITE "${context_header}" "${patched_context_source}")
    # Quill is header-only. Do not execute upstream CMake, tests, examples or install rules.
    add_library(better_pull_review_quill INTERFACE)
    add_library(quill::quill ALIAS better_pull_review_quill)
    target_include_directories(better_pull_review_quill SYSTEM INTERFACE "${stage}/quill-13.0.0/include")
    target_link_libraries(better_pull_review_quill INTERFACE Threads::Threads)
endfunction()
