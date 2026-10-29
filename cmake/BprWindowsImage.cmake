# [[BPR-0974]]. The identity a shipped Windows image states about itself.
#
# BPR-1110 found the image had no version resource: no company, product,
# description or version, where every Qt DLL beside it has one. The owner chose
# to add it (decision 5c30287a, 2026-09-25). The values come from the project's
# own name and version and the vendor CPack names, so nothing here is invented;
# no copyright or licence statement is made, because the owner has not approved
# distribution terms.

# bpr_add_windows_version_resource(<target> <description>)
#
# Gives <target> a VERSIONINFO resource on Windows; elsewhere it does nothing.
# OriginalFilename is the file the target links, from its OUTPUT_NAME.
function(bpr_add_windows_version_resource target description)
    if(NOT WIN32)
        return()
    endif()
    get_target_property(output_name "${target}" OUTPUT_NAME)
    if(NOT output_name)
        set(output_name "${target}")
    endif()
    set(BPR_VERSION_RESOURCE_DESCRIPTION "${description}")
    set(BPR_VERSION_RESOURCE_INTERNAL_NAME "${output_name}")
    set(BPR_VERSION_RESOURCE_FILE_NAME "${output_name}${CMAKE_EXECUTABLE_SUFFIX}")
    if(NOT DEFINED BPR_RELEASE_BUILD_NUMBER)
        set(BPR_RELEASE_BUILD_NUMBER 0)
    endif()
    if(NOT BPR_RELEASE_DISPLAY_VERSION)
        set(BPR_RELEASE_DISPLAY_VERSION "${PROJECT_VERSION}")
    endif()
    set(BPR_VERSION_RESOURCE_NUMBER
        "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${BPR_RELEASE_BUILD_NUMBER}")
    foreach(value IN ITEMS BPR_VENDOR_NAME PROJECT_NAME PROJECT_VERSION BPR_VERSION_RESOURCE_DESCRIPTION)
        if("${${value}}" STREQUAL "" OR "${${value}}" MATCHES "[\"\\\\]")
            message(FATAL_ERROR "${value} cannot be stated in a version resource: '${${value}}'")
        endif()
    endforeach()
    set(resource "${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc")
    configure_file(
        "${PROJECT_SOURCE_DIR}/packaging/windows/version.rc.in"
        "${resource}"
        @ONLY
    )
    target_sources("${target}" PRIVATE "${resource}")
endfunction()
