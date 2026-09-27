if(BPR_BUILD_QT OR BPR_BUILD_TOUR_CLI)
    add_library(
        better_pull_review_adapter_git
        STATIC
            src/installed_git_cache.cpp
            src/installed_git_process.cpp
    )
    add_library(
        BetterPullReview::AdapterGit
        ALIAS
        better_pull_review_adapter_git
    )
    target_include_directories(
        better_pull_review_adapter_git
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(
        better_pull_review_adapter_git
        PUBLIC
            BetterPullReview::Application
        PRIVATE
            glaze::glaze
            Qt6::Core
            Threads::Threads
    )
    target_compile_features(
        better_pull_review_adapter_git
        PUBLIC
            cxx_std_23
    )
    if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(
            better_pull_review_adapter_git
            PUBLIC
                _LIBCPP_ENABLE_EXPERIMENTAL
        )
    endif()
    if(WIN32)
        # [[BPR-0622]]. The ownership journal applies the same header-only
        # Windows guard and private-object rules as the agent supervisor.
        # [[BPR-1115]]. An unguarded Git takes the platform target's launch
        # restriction.
        target_link_libraries(better_pull_review_adapter_git PRIVATE BetterPullReview::AdapterAgent BetterPullReview::Platform Advapi32)
    endif()
    # [[BPR-0622]]. A guarded Git launch runs inside the bundled supervisor on
    # Windows too.
    if(UNIX OR WIN32)
        add_dependencies(better_pull_review_adapter_git tool_better_pull_review_agent_supervisor)
        if(BUILD_TESTING)
            target_compile_definitions(better_pull_review_adapter_git PRIVATE
                "BPR_AGENT_SUPERVISOR_EXECUTABLE=\"$<TARGET_FILE:tool_better_pull_review_agent_supervisor>\"")
        endif()
    endif()
    bpr_enable_hardening(better_pull_review_adapter_git)
    bpr_enable_warnings(better_pull_review_adapter_git)
else()
    add_library(better_pull_review_adapter_git INTERFACE)
    add_library(
        BetterPullReview::AdapterGit
        ALIAS
        better_pull_review_adapter_git
    )
    target_include_directories(
        better_pull_review_adapter_git
        INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(
        better_pull_review_adapter_git
        INTERFACE
            BetterPullReview::Application
    )
endif()
