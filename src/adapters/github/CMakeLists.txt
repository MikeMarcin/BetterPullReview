add_library(better_pull_review_adapter_github INTERFACE)
add_library(BetterPullReview::AdapterGitHub ALIAS better_pull_review_adapter_github)

target_link_libraries(
    better_pull_review_adapter_github
    INTERFACE
        BetterPullReview::Application
)
