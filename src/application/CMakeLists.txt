add_library(
    better_pull_review_application
    STATIC
        src/github_capability_cache.cpp
        src/github_application_host.cpp
        src/github_credential_vault.cpp
        src/host.cpp
        src/manual_executor.cpp
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

bpr_enable_hardening(better_pull_review_application)
bpr_enable_warnings(better_pull_review_application)
