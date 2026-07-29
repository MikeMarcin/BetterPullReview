add_library(better_pull_review_adapter_github INTERFACE)
add_library(BetterPullReview::AdapterGitHub ALIAS better_pull_review_adapter_github)

add_library(better_pull_review_adapter_git INTERFACE)
add_library(BetterPullReview::AdapterGit ALIAS better_pull_review_adapter_git)

add_library(better_pull_review_adapter_sqlite INTERFACE)
add_library(BetterPullReview::AdapterSQLite ALIAS better_pull_review_adapter_sqlite)

add_library(better_pull_review_adapter_ai INTERFACE)
add_library(BetterPullReview::AdapterAI ALIAS better_pull_review_adapter_ai)

foreach(
    adapter
    IN ITEMS
        better_pull_review_adapter_github
        better_pull_review_adapter_git
        better_pull_review_adapter_sqlite
        better_pull_review_adapter_ai
)
    target_link_libraries("${adapter}" INTERFACE BetterPullReview::Application)
endforeach()

if(BPR_BUILD_QT)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
    add_subdirectory(qt)
endif()
