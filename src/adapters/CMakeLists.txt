add_subdirectory(logging)
add_subdirectory(github)
add_subdirectory(git)
add_subdirectory(sqlite)
add_subdirectory(workspace)
add_subdirectory(syntax)
add_subdirectory(agent)
add_subdirectory(mcp)
add_subdirectory(update)

add_library(better_pull_review_adapter_ai INTERFACE)
add_library(BetterPullReview::AdapterAI ALIAS better_pull_review_adapter_ai)

foreach(
    adapter
    IN ITEMS
        better_pull_review_adapter_ai
)
    target_link_libraries("${adapter}" INTERFACE BetterPullReview::Application)
endforeach()

if(BPR_BUILD_QT)
    add_subdirectory(qt)
endif()
