include_guard(GLOBAL)

set(BPR_CPM_VERSION "0.43.1")
set(
    BPR_CPM_SHA256
    "1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934"
)

function(bpr_load_cpm)
    set(bpr_cpm_options
        CPM_USE_LOCAL_PACKAGES
        CPM_LOCAL_PACKAGES_ONLY
        CPM_DOWNLOAD_ALL
        CPM_DONT_UPDATE_MODULE_PATH
        CPM_DONT_CREATE_PACKAGE_LOCK
        CPM_INCLUDE_ALL_IN_PACKAGE_LOCK
        CPM_USE_NAMED_CACHE_DIRECTORIES
    )
    foreach(option IN LISTS bpr_cpm_options)
        if(NOT DEFINED ${option})
            set(${option} OFF CACHE BOOL "CPM ${option} default" FORCE)
        endif()
        if(DEFINED ENV{${option}})
            set("bpr_saved_${option}" "$ENV{${option}}")
            set("bpr_saved_${option}_defined" TRUE)
        else()
            set("bpr_saved_${option}_defined" FALSE)
        endif()
        set(ENV{${option}} "${${option}}")
    endforeach()
    if(NOT CPM_SOURCE_CACHE)
        set(
            CPM_SOURCE_CACHE
            "${CMAKE_BINARY_DIR}/_cpm-cache"
            CACHE PATH
            "Shared CPM source cache"
            FORCE
        )
    endif()
    file(MAKE_DIRECTORY "${CPM_SOURCE_CACHE}/cpm")
    set(cpm_script "${CPM_SOURCE_CACHE}/cpm/CPM-${BPR_CPM_VERSION}.cmake")
    set(cpm_lock "${cpm_script}.lock")
    file(
        LOCK "${cpm_lock}"
        GUARD FUNCTION
        TIMEOUT 120
        RESULT_VARIABLE lock_result
    )
    if(lock_result)
        message(
            FATAL_ERROR
            "Unable to lock the shared CPM.cmake cache: ${lock_result}"
        )
    endif()

    if(EXISTS "${cpm_script}")
        file(SHA256 "${cpm_script}" actual_sha256)
        if(NOT actual_sha256 STREQUAL BPR_CPM_SHA256)
            message(FATAL_ERROR "Cached CPM.cmake ${BPR_CPM_VERSION} has the wrong SHA-256")
        endif()
    elseif(BPR_DEPENDENCY_OFFLINE)
        message(
            FATAL_ERROR
            "BPR_DEPENDENCY_OFFLINE=ON requires cached CPM.cmake ${BPR_CPM_VERSION}: "
            "'${cpm_script}'"
        )
    else()
        set(download "${cpm_script}.download")
        file(
            DOWNLOAD
            "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${BPR_CPM_VERSION}/CPM.cmake"
            "${download}"
            EXPECTED_HASH "SHA256=${BPR_CPM_SHA256}"
            STATUS status
            TLS_VERIFY ON
        )
        list(GET status 0 status_code)
        list(GET status 1 status_message)
        if(NOT status_code EQUAL 0)
            file(REMOVE "${download}")
            message(FATAL_ERROR "Unable to download verified CPM.cmake: ${status_message}")
        endif()
        file(RENAME "${download}" "${cpm_script}")
    endif()

    if(BPR_DEPENDENCY_OFFLINE)
        set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Offline dependency mode" FORCE)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Offline dependency mode" FORCE)
    endif()
    include("${cpm_script}")
    foreach(option IN LISTS bpr_cpm_options)
        if(bpr_saved_${option}_defined)
            set(ENV{${option}} "${bpr_saved_${option}}")
        else()
            unset(ENV{${option}})
        endif()
    endforeach()
endfunction()
