add_library(better_pull_review_application INTERFACE)
add_library(BetterPullReview::Application ALIAS better_pull_review_application)

target_link_libraries(
    better_pull_review_application
    INTERFACE
        BetterPullReview::Core
)
target_include_directories(
    better_pull_review_application
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_application INTERFACE cxx_std_23)
