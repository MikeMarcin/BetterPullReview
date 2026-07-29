add_library(better_pull_review_composition INTERFACE)
add_library(BetterPullReview::Composition ALIAS better_pull_review_composition)

target_link_libraries(
    better_pull_review_composition
    INTERFACE
        BetterPullReview::Application
        BetterPullReview::AdapterGitHub
        BetterPullReview::AdapterGit
        BetterPullReview::AdapterSQLite
        BetterPullReview::AdapterAI
)

if(BPR_BUILD_QT)
    target_link_libraries(
        better_pull_review_composition
        INTERFACE
            BetterPullReview::AdapterQt
    )
endif()
