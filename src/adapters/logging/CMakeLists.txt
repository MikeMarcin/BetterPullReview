if(BPR_BUILD_QT)
    add_library(better_pull_review_adapter_logging STATIC src/quill_log.cpp src/session_files.cpp)
    add_library(BetterPullReview::AdapterLogging ALIAS better_pull_review_adapter_logging)
    target_include_directories(better_pull_review_adapter_logging PUBLIC include)
    target_link_libraries(better_pull_review_adapter_logging PUBLIC BetterPullReview::Application PRIVATE quill::quill)
    # A content identity also distinguishes uncommitted developer candidates.
    file(GLOB_RECURSE bpr_diagnostic_sources CONFIGURE_DEPENDS
        RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp" "${PROJECT_SOURCE_DIR}/src/CMakeLists.txt"
        "${PROJECT_SOURCE_DIR}/src/*/CMakeLists.txt")
    list(SORT bpr_diagnostic_sources)
    set(bpr_diagnostic_manifest "${PROJECT_VERSION}\n${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}\n${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}\n")
    foreach(source IN LISTS bpr_diagnostic_sources)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${source}")
        file(SHA256 "${PROJECT_SOURCE_DIR}/${source}" source_digest)
        string(APPEND bpr_diagnostic_manifest "${source}=${source_digest}\n")
    endforeach()
    string(SHA256 bpr_diagnostic_build "${bpr_diagnostic_manifest}")
    set(bpr_diagnostic_release "")
    if(BPR_RELEASE_BUILD_NUMBER)
        set(bpr_diagnostic_build "${BPR_RELEASE_COHORT_DIGEST}")
        set(bpr_diagnostic_release "-build.${BPR_RELEASE_BUILD_NUMBER}-${BPR_RELEASE_CONFIGURATION_DIGEST}")
    endif()
    target_compile_definitions(better_pull_review_adapter_logging PRIVATE
        "BPR_DIAGNOSTIC_BUILD=\"${PROJECT_VERSION}-${bpr_diagnostic_build}${bpr_diagnostic_release}-$<CONFIG>\"")
    target_compile_features(better_pull_review_adapter_logging PUBLIC cxx_std_23)
    if(WIN32)
        target_link_libraries(better_pull_review_adapter_logging PRIVATE Advapi32)
        # [[BPR-0825]]. The native log is created with the platform target's
        # one owner-private descriptor (src/adapters/native).
        target_link_libraries(better_pull_review_adapter_logging PRIVATE BetterPullReview::Platform)
    endif()
    bpr_enable_hardening(better_pull_review_adapter_logging)
endif()
