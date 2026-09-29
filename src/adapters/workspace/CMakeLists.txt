# [[BPR-1148]]. The read-only local workspace source for Review Cycles. It
# opens a deliberately chosen folder, file or Git checkout and reads beneath it
# without following links, writing, running Git or reaching the network. It
# links the application ports and nothing else: no Qt, no Git process, no
# provider, no agent.
add_library(
    better_pull_review_adapter_workspace
    STATIC
        src/local_workspace_source.cpp
)
add_library(
    BetterPullReview::AdapterWorkspace
    ALIAS
    better_pull_review_adapter_workspace
)
target_include_directories(
    better_pull_review_adapter_workspace
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_workspace
    PUBLIC
        BetterPullReview::Application
)
target_compile_features(
    better_pull_review_adapter_workspace
    PUBLIC
        cxx_std_23
)
bpr_enable_hardening(better_pull_review_adapter_workspace)
bpr_enable_warnings(better_pull_review_adapter_workspace)
