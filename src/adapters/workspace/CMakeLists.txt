# [[BPR-1148]]. The read-only local workspace source for local drafts. It
# opens a deliberately chosen folder, file or Git checkout and reads beneath it
# without following links, writing, running Git or reaching the network. It
# links the application ports and nothing else: no Qt, no Git process, no
# provider, no agent.
add_library(
    better_pull_review_adapter_workspace
    STATIC
        src/local_workspace_source.cpp
        # [[BPR-1750]]. RFC-BPR-0092 D2: clone folders, read by
        # metadata, no links followed, nothing written or run.
        src/clone_folder_scan.cpp
        # [[BPR-2303]]. An app bundle chosen as an agent's program: its
        # Info.plist and Contents/MacOS listing, no links followed.
        src/app_bundle_read.cpp
)
if(APPLE)
    target_link_libraries(better_pull_review_adapter_workspace PRIVATE "-framework CoreFoundation")
endif()
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
