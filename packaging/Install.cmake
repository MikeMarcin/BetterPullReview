# Application-only install graph. No core SDK/export surface is implied.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR BUILD_TESTING)
    message(FATAL_ERROR "Packages require a Release, BUILD_TESTING=OFF graph")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
get_filename_component(bpr_package_qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
set(bpr_package_qt_version "${BPR_PRODUCT_QT_VERSION}")
# [[BPR-1179]]. The helpers a package ships follow its component families: the
# supervisor in every package (Git's guarded launches run inside it), the MCP
# context helper only with agent integrations. The record names both families,
# so a release check reads which package it holds rather than inferring it.
set(bpr_package_helpers tool_better_pull_review_agent_supervisor)
set(bpr_package_agent_integrations false)
if(BPR_BUILD_AGENT_INTEGRATIONS)
    list(APPEND bpr_package_helpers tool_better_pull_review_mcp_context_helper)
    set(bpr_package_agent_integrations true)
endif()
set(bpr_package_local_automation false)
if(BPR_BUILD_LOCAL_AUTOMATION)
    set(bpr_package_local_automation true)
endif()
# The artifact's name says which families it omits. Its package identity (name,
# Windows upgrade code, macOS bundle) is the same for every variant, so
# installing one over another replaces the program files and nothing else.
set(bpr_package_variant "")
if(NOT BPR_BUILD_AGENT_INTEGRATIONS)
    string(APPEND bpr_package_variant "-agent-free")
endif()
if(NOT BPR_BUILD_LOCAL_AUTOMATION)
    string(APPEND bpr_package_variant "-automation-off")
endif()
set(bpr_package_helper_files "")
set(bpr_package_target "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/packaging/package_runtime.py" baseline
        OUTPUT_VARIABLE bpr_package_target OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE bpr_package_baseline_result)
    if(NOT bpr_package_baseline_result EQUAL 0)
        message(FATAL_ERROR "Linux distribution packaging requires a supported native baseline")
    endif()
endif()
foreach(bpr_package_helper IN LISTS bpr_package_helpers)
    list(APPEND bpr_package_helper_files "\"$<TARGET_FILE:${bpr_package_helper}>\"")
endforeach()
list(JOIN bpr_package_helper_files "," bpr_package_helper_files)
# Native allocator courses need their actual runtime closure before final AC07.
set(bpr_package_allocator "")
if(APPLE AND BPR_MIMALLOC_ACTIVE)
    set(bpr_package_allocator ",\"Contents/Frameworks/$<TARGET_FILE_NAME:mimalloc>\":\"$<TARGET_FILE:mimalloc>\"")
elseif(WIN32 AND BPR_MIMALLOC_ACTIVE)
    set(bpr_package_allocator ",\"$<TARGET_FILE_NAME:mimalloc>\":\"$<TARGET_FILE:mimalloc>\",\"mimalloc-redirect.dll\":\"$<TARGET_FILE_DIR:mimalloc>/mimalloc-redirect.dll\"")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BPR_MIMALLOC_ACTIVE)
    set(bpr_package_allocator ",\"$<TARGET_SONAME_FILE_NAME:mimalloc>\":\"$<TARGET_FILE:mimalloc>\"")
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/_bpr/package-inputs.json" CONTENT
    "{\"schema\":\"bpr.package-inputs.v1\",\"platform\":\"${CMAKE_SYSTEM_NAME}\",\"version\":\"${PROJECT_VERSION}\",\"qt_version\":\"${bpr_package_qt_version}\",\"qt_root\":\"${bpr_package_qt_root}\",\"source\":\"${PROJECT_SOURCE_DIR}\",\"cache\":\"${CPM_SOURCE_CACHE}\",\"app\":\"$<TARGET_FILE:app_better_pull_review>\",\"helpers\":[${bpr_package_helper_files}],\"private_libraries\":{\"$<$<PLATFORM_ID:Darwin>:Contents/Frameworks/>$<TARGET_FILE_NAME:better_pull_review_xdiff>\":\"$<TARGET_FILE:better_pull_review_xdiff>\"${bpr_package_allocator}},\"families\":{\"agent_integrations\":${bpr_package_agent_integrations},\"local_automation\":${bpr_package_local_automation}},\"variant\":\"${bpr_package_variant}\",\"target\":\"${bpr_package_target}\"}\n")
if(APPLE)
    # tools/release.sh remains the sole macOS deployment/DMG/ZIP pipeline.
    return()
endif()
if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MSVC)
        message(FATAL_ERROR "Windows packages require the native MSVC x64 graph")
    endif()
    set(bpr_runtime_destination ".")
else()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        message(FATAL_ERROR "Linux packages require an x86_64 baseline host")
    endif()
    if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
        message(FATAL_ERROR "Managed Linux packages install beneath /usr")
    endif()
    set(bpr_runtime_destination "lib/betterpullreview")
    set_target_properties(app_better_pull_review ${bpr_package_helpers} better_pull_review_xdiff PROPERTIES INSTALL_RPATH "$ORIGIN")
    install(PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/linux/better_pull_review"
        DESTINATION bin COMPONENT Runtime)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/icons/linux/io.github.MikeMarcin.BetterPullReview.desktop"
        DESTINATION share/applications COMPONENT Runtime)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/icons/better_pull_review.svg"
        DESTINATION share/icons/hicolor/scalable/apps RENAME io.github.MikeMarcin.BetterPullReview.svg COMPONENT Runtime)
endif()
install(TARGETS app_better_pull_review ${bpr_package_helpers}
    RUNTIME DESTINATION "${bpr_runtime_destination}" COMPONENT Runtime)
# [[BPR-1485]]. The linked xdiff is its own shared library, installed beside
# the application as the private Qt libraries are (macOS: tools/release.sh puts
# it in Contents/Frameworks). The application loads it through $ORIGIN on Linux
# and from its own directory on Windows.
# Its Windows import library is a build input only and never ships.
install(TARGETS better_pull_review_xdiff
    RUNTIME DESTINATION "${bpr_runtime_destination}" COMPONENT Runtime
    LIBRARY DESTINATION "${bpr_runtime_destination}" COMPONENT Runtime
    ARCHIVE DESTINATION "_bpr/unpackaged" COMPONENT Unpackaged EXCLUDE_FROM_ALL)
if(WIN32 AND BPR_MIMALLOC_ACTIVE)
    install(FILES "$<TARGET_FILE:mimalloc>" "$<TARGET_FILE_DIR:mimalloc>/mimalloc-redirect.dll"
        DESTINATION "${bpr_runtime_destination}" COMPONENT Runtime)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND BPR_MIMALLOC_ACTIVE)
    install(FILES "$<TARGET_FILE:mimalloc>" DESTINATION "${bpr_runtime_destination}"
        RENAME "$<TARGET_SONAME_FILE_NAME:mimalloc>" COMPONENT Runtime)
endif()
if(BPR_EXTERNAL_QT_RESOURCES)
    # [[BPR-0974]]. The image's Qt resources ship beside it, and it refuses to
    # start without exactly this file (cmake/BprQtResources.cmake). Installed
    # before the deployment script, which declares it as runtime data.
    install(FILES "$<TARGET_FILE_DIR:app_better_pull_review>/${BPR_QT_RESOURCE_BUNDLE_NAME}"
        DESTINATION "${bpr_runtime_destination}" COMPONENT Runtime)
endif()
configure_file("${PROJECT_SOURCE_DIR}/packaging/DeployRuntime.cmake.in"
    "${CMAKE_BINARY_DIR}/_bpr/DeployRuntime.cmake" @ONLY)
install(SCRIPT "${CMAKE_BINARY_DIR}/_bpr/DeployRuntime.cmake" COMPONENT Runtime)

set(CPACK_PACKAGE_NAME BetterPullReview)
set(CPACK_PACKAGE_VENDOR "${BPR_VENDOR_NAME}")
set(CPACK_PACKAGE_CONTACT "https://github.com/MikeMarcin/BetterPullReview")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native GitHub pull-request review workbench")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/DISTRIBUTION.txt")
if(WIN32)
    set(CPACK_GENERATOR WIX)
    set(CPACK_WIX_VERSION 4)
    set(CPACK_WIX_ARCHITECTURE x64)
    set(CPACK_WIX_INSTALL_SCOPE perUser)
    set(CPACK_WIX_UPGRADE_GUID "18DF84A8-5883-4979-B1C6-6F226934AA68")
    set(CPACK_WIX_ROOT_FOLDER_ID LocalAppDataFolder)
    set(CPACK_WIX_PATCH_FILE "${PROJECT_SOURCE_DIR}/packaging/windows/InstallRoot.xml")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Programs/BetterPullReview")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/packaging/icons/windows/better_pull_review.ico")
    set(CPACK_PACKAGE_EXECUTABLES "better_pull_review" "BetterPullReview")
    set(CPACK_PACKAGE_FILE_NAME "BetterPullReview-${PROJECT_VERSION}-windows-x64${bpr_package_variant}")
else()
    if(bpr_package_target STREQUAL "linux-ubuntu-26.04-x64")
        set(CPACK_GENERATOR DEB)
    else()
        # Retained optional RHEL producer; not part of the selected release set.
        set(CPACK_GENERATOR RPM)
    endif()
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
    set(CPACK_DEBIAN_PACKAGE_NAME betterpullreview)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "git (>= 1:2.39), libsecret-1-0 (>= 0.20), libssl3t64, xwayland")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "/usr/lib/betterpullreview")
    set(CPACK_RPM_PACKAGE_NAME betterpullreview)
    set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
    # The owner has not approved first-party distribution terms. Do not invent
    # them or mislabel the separately licensed embedded syntax definitions.
    set(CPACK_RPM_PACKAGE_LICENSE "NOASSERTION")
    set(CPACK_RPM_PACKAGE_REQUIRES "git >= 2.39, libsecret >= 0.20, openssl-libs, xorg-x11-server-Xwayland")
    set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
    set(CPACK_PACKAGE_FILE_NAME "BetterPullReview-${PROJECT_VERSION}-${bpr_package_target}${bpr_package_variant}")
endif()
include(CPack)
