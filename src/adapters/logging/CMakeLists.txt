if(BPR_BUILD_QT)
    add_library(better_pull_review_adapter_logging STATIC src/quill_log.cpp)
    add_library(BetterPullReview::AdapterLogging ALIAS better_pull_review_adapter_logging)
    target_include_directories(better_pull_review_adapter_logging PUBLIC include)
    target_link_libraries(better_pull_review_adapter_logging PUBLIC BetterPullReview::Application PRIVATE quill::quill)
    target_compile_features(better_pull_review_adapter_logging PUBLIC cxx_std_23)
    if(WIN32)
        target_link_libraries(better_pull_review_adapter_logging PRIVATE Advapi32)
        # [[BPR-0825]]. The native log is created with the platform target's
        # one owner-private descriptor (src/adapters/native).
        target_link_libraries(better_pull_review_adapter_logging PRIVATE BetterPullReview::Platform)
    endif()
    bpr_enable_hardening(better_pull_review_adapter_logging)
endif()
