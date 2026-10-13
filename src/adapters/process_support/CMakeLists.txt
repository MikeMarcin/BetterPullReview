# [[BPR-1436]]. QProcess supervision the gh and Git runners share: the
# serialized worker lane with its quarantine, and the process-group wait, kill,
# resolution, kill-and-reap, background reap and abandonment. Adapters may not
# depend on each other and the platform target is header-only without Qt, so
# this is a Qt Core static target of its own. It depends on no first-party
# layer and decides nothing about what a process runs.
if(BPR_BUILD_QT OR BPR_BUILD_TOUR_CLI)
    add_library(
        better_pull_review_process_support
        STATIC
            src/process_supervision.cpp
            include/bpr/adapters/process_support/process_supervision.hpp
    )
    add_library(
        BetterPullReview::ProcessSupport
        ALIAS
        better_pull_review_process_support
    )
    target_include_directories(
        better_pull_review_process_support
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(
        better_pull_review_process_support
        PUBLIC
            Qt6::Core
            Threads::Threads
    )
    target_compile_features(
        better_pull_review_process_support
        PUBLIC
            cxx_std_23
    )
    if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(
            better_pull_review_process_support
            PUBLIC
                _LIBCPP_ENABLE_EXPERIMENTAL
        )
    endif()
    bpr_enable_hardening(better_pull_review_process_support)
    bpr_enable_warnings(better_pull_review_process_support)
endif()
