# [[BPR-1485]]. The diff engine's primary line matcher: Git 2.56.0's xdiff
# behind the application's diff_line_matcher_port (RFC-BPR-0082). This adapter
# is the only target that links the bpr_xdiff shared library, and
# src/xdiff_line_matcher.cpp the only source that calls it. It links the
# application ports and nothing else: no Qt, no process, no filesystem.
add_library(
    better_pull_review_adapter_xdiff
    STATIC
        src/xdiff_line_matcher.cpp
)
add_library(
    BetterPullReview::AdapterXdiff
    ALIAS
    better_pull_review_adapter_xdiff
)
target_include_directories(
    better_pull_review_adapter_xdiff
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_xdiff
    PUBLIC
        BetterPullReview::Application
    PRIVATE
        BetterPullReview::Xdiff
)
target_compile_features(
    better_pull_review_adapter_xdiff
    PUBLIC
        cxx_std_23
)
bpr_enable_hardening(better_pull_review_adapter_xdiff)
bpr_enable_warnings(better_pull_review_adapter_xdiff)
