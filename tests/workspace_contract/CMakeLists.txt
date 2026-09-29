# [[BPR-1148]]. Review Cycles workspace contracts: the read-only local source
# against disposable roots on the native filesystem. When the build found Git,
# that Git builds repository fixtures; the product under test never runs it.
set(bpr_workspace_git_arguments "")
if(TARGET Git::Git)
    set(bpr_workspace_git_arguments --git "$<TARGET_FILE:Git::Git>")
endif()

bpr_add_executable(test_workspace_source workspace_source_tests.cpp)
target_link_libraries(test_workspace_source PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_workspace_source)
bpr_enable_warnings(test_workspace_source)
bpr_add_network_isolated_test(
    NAME workspace_source
    WINDOWS_UNCONTAINED
    COMMAND test_workspace_source ${bpr_workspace_git_arguments}
)

# [[BPR-1149]] and [[BPR-1150]]. Capture against real roots into the real store.
bpr_add_executable(test_review_cycle_capture review_cycle_capture_tests.cpp)
target_link_libraries(test_review_cycle_capture PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_capture)
bpr_enable_warnings(test_review_cycle_capture)
bpr_add_network_isolated_test(
    NAME review_cycle_capture
    WINDOWS_UNCONTAINED
    COMMAND test_review_cycle_capture ${bpr_workspace_git_arguments}
)
