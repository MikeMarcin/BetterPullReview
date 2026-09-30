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
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_workspace_source PRIVATE advapi32 BetterPullReview::Platform)
endif()
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
if(WIN32)
    target_link_libraries(test_review_cycle_capture PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_capture)
bpr_add_network_isolated_test(
    NAME review_cycle_capture
    WINDOWS_UNCONTAINED
    COMMAND test_review_cycle_capture ${bpr_workspace_git_arguments}
)

# [[BPR-1151]]. Comparison views over retained cycle material.
bpr_add_executable(test_review_cycle_comparison review_cycle_comparison_tests.cpp)
target_link_libraries(test_review_cycle_comparison PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_cycle_comparison)
if(WIN32)
    target_link_libraries(test_review_cycle_comparison PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_comparison)
bpr_add_network_isolated_test(NAME review_cycle_comparison WINDOWS_UNCONTAINED COMMAND test_review_cycle_comparison)

# [[BPR-1151]]. Scale measurement: capture, recapture, paging and diffs over a
# synthetic tree. The row runs a small tree and asserts only correctness and
# bounds; `--files 100000` is the LIMITS measurement, run by hand.
bpr_add_executable(bench_review_cycle_scale review_cycle_scale_bench.cpp)
target_link_libraries(bench_review_cycle_scale PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(bench_review_cycle_scale)
if(WIN32)
    target_link_libraries(bench_review_cycle_scale PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(bench_review_cycle_scale)
bpr_add_network_isolated_test(NAME review_cycle_scale WINDOWS_UNCONTAINED COMMAND bench_review_cycle_scale --files 2000)

# [[BPR-1152]]. Capture recovery and retained-material budgets. The kill course
# forks and delivers SIGKILL, so it runs on POSIX only; the rest runs everywhere.
bpr_add_executable(test_review_cycle_recovery review_cycle_recovery_tests.cpp)
target_link_libraries(test_review_cycle_recovery PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_cycle_recovery)
if(WIN32)
    target_link_libraries(test_review_cycle_recovery PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_recovery)
bpr_add_network_isolated_test(NAME review_cycle_recovery WINDOWS_UNCONTAINED COMMAND test_review_cycle_recovery)

# [[BPR-1153]]. Private cycle feedback on retained material through the runtime.
bpr_add_executable(test_review_cycle_feedback review_cycle_feedback_tests.cpp)
target_link_libraries(test_review_cycle_feedback PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_feedback)
if(WIN32)
    target_link_libraries(test_review_cycle_feedback PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_feedback)
bpr_add_network_isolated_test(NAME review_cycle_feedback WINDOWS_UNCONTAINED COMMAND test_review_cycle_feedback)

# [[BPR-1155]]. Exact suggestion records, previews and preflight on retained material.
bpr_add_executable(test_review_cycle_suggestion review_cycle_suggestion_tests.cpp)
target_link_libraries(test_review_cycle_suggestion PRIVATE BetterPullReview::AdapterWorkspace BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_suggestion)
if(WIN32)
    target_link_libraries(test_review_cycle_suggestion PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_suggestion)
bpr_add_network_isolated_test(NAME review_cycle_suggestion WINDOWS_UNCONTAINED COMMAND test_review_cycle_suggestion)
