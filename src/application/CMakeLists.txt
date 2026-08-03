add_library(
    better_pull_review_application
    STATIC
        src/appearance_application_host.cpp
        src/diff_application_host.cpp
        src/github_capability_cache.cpp
        src/epoch_application_host.cpp
        src/epoch_runtime_ports.cpp
        src/github_application_host.cpp
        src/github_credential_vault.cpp
        src/github_projection_mapping.cpp
        src/git_protocol_v2.cpp
        src/git_smart_transport_port.cpp
        src/host.cpp
        src/manual_executor.cpp
        src/workbench_application_host.cpp
)
add_library(BetterPullReview::Application ALIAS better_pull_review_application)

target_link_libraries(
    better_pull_review_application
    PUBLIC
        BetterPullReview::Core
)
target_include_directories(
    better_pull_review_application
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_application PUBLIC cxx_std_23)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_application
        PUBLIC
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
if(WIN32)
    target_link_libraries(
        better_pull_review_application
        PRIVATE
            Bcrypt
    )
endif()

if(BUILD_TESTING)
    target_compile_definitions(
        better_pull_review_application
        PRIVATE
            BPR_APPLICATION_TESTING=1
    )
endif()

bpr_enable_hardening(better_pull_review_application)
bpr_enable_warnings(better_pull_review_application)
