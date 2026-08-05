bpr_add_executable(
    test_better_pull_review_core_tests
    foundation_tests.cpp
)

target_link_libraries(
    test_better_pull_review_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_core_tests)
bpr_enable_warnings(test_better_pull_review_core_tests)

add_test(NAME core_foundation COMMAND test_better_pull_review_core_tests)

bpr_add_executable(
    test_better_pull_review_github_core_tests
    github_policy_tests.cpp
)

target_link_libraries(
    test_better_pull_review_github_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_github_core_tests)
bpr_enable_warnings(test_better_pull_review_github_core_tests)

add_test(
    NAME core_github_policy
    COMMAND test_better_pull_review_github_core_tests
)

bpr_add_executable(
    test_better_pull_review_epoch_core_tests
    epoch_policy_tests.cpp
)

target_link_libraries(
    test_better_pull_review_epoch_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_epoch_core_tests)
bpr_enable_warnings(test_better_pull_review_epoch_core_tests)

add_test(
    NAME core_epoch_policy
    COMMAND test_better_pull_review_epoch_core_tests
)

bpr_add_executable(
    test_better_pull_review_diff_core_tests
    diff_model_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_core_tests)

add_test(
    NAME core_diff_model
    COMMAND test_better_pull_review_diff_core_tests
)

bpr_add_executable(
    test_better_pull_review_diff_compute_core_tests
    diff_compute_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_compute_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_compute_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_compute_core_tests)

add_test(
    NAME core_diff_compute
    COMMAND test_better_pull_review_diff_compute_core_tests
)

bpr_add_executable(
    test_better_pull_review_diff_semantics_core_tests
    diff_semantics_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_semantics_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_semantics_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_semantics_core_tests)

add_test(
    NAME core_diff_semantics
    COMMAND test_better_pull_review_diff_semantics_core_tests
)

bpr_add_executable(
    test_better_pull_review_diff_transition_core_tests
    diff_transition_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_transition_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_transition_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_transition_core_tests)

add_test(
    NAME core_diff_transition
    COMMAND test_better_pull_review_diff_transition_core_tests
)

bpr_add_executable(
    test_better_pull_review_workbench_interaction_core_tests
    workbench_interaction_tests.cpp
)

target_link_libraries(
    test_better_pull_review_workbench_interaction_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_workbench_interaction_core_tests)
bpr_enable_warnings(test_better_pull_review_workbench_interaction_core_tests)

add_test(
    NAME core_workbench_interaction
    COMMAND test_better_pull_review_workbench_interaction_core_tests
)

bpr_add_executable(
    test_better_pull_review_appearance_core_tests
    appearance_model_tests.cpp
)

target_link_libraries(
    test_better_pull_review_appearance_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_appearance_core_tests)
bpr_enable_warnings(test_better_pull_review_appearance_core_tests)

add_test(
    NAME core_appearance_policy
    COMMAND test_better_pull_review_appearance_core_tests
)

bpr_add_executable(
    test_better_pull_review_appearance_contrast_core_tests
    appearance_contrast_tests.cpp
)

target_link_libraries(
    test_better_pull_review_appearance_contrast_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_appearance_contrast_core_tests)
bpr_enable_warnings(test_better_pull_review_appearance_contrast_core_tests)

add_test(
    NAME core_appearance_contrast
    COMMAND test_better_pull_review_appearance_contrast_core_tests
)

bpr_add_executable(
    test_better_pull_review_theme_exchange_core_tests
    theme_exchange_tests.cpp
)

target_link_libraries(
    test_better_pull_review_theme_exchange_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_theme_exchange_core_tests)
bpr_enable_warnings(test_better_pull_review_theme_exchange_core_tests)

add_test(
    NAME core_theme_exchange
    COMMAND test_better_pull_review_theme_exchange_core_tests
)

bpr_add_executable(
    test_better_pull_review_review_ledger_core_tests
    review_ledger_tests.cpp
)

target_link_libraries(
    test_better_pull_review_review_ledger_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_ledger_core_tests)
bpr_enable_warnings(test_better_pull_review_review_ledger_core_tests)

add_test(
    NAME core_review_ledger
    COMMAND test_better_pull_review_review_ledger_core_tests
)

bpr_add_executable(
    test_better_pull_review_review_round_trip_core_tests
    review_round_trip_tests.cpp
)

target_link_libraries(
    test_better_pull_review_review_round_trip_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_round_trip_core_tests)
bpr_enable_warnings(test_better_pull_review_review_round_trip_core_tests)

add_test(
    NAME core_review_round_trip
    COMMAND test_better_pull_review_review_round_trip_core_tests
)

bpr_add_executable(
    test_better_pull_review_defect_workflow_core_tests
    defect_workflow_tests.cpp
)

target_link_libraries(
    test_better_pull_review_defect_workflow_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_defect_workflow_core_tests)
bpr_enable_warnings(test_better_pull_review_defect_workflow_core_tests)

add_test(
    NAME core_defect_workflow
    COMMAND test_better_pull_review_defect_workflow_core_tests
)

bpr_add_executable(
    test_better_pull_review_review_queue_core_tests
    review_queue_tests.cpp
)

target_link_libraries(
    test_better_pull_review_review_queue_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_queue_core_tests)
bpr_enable_warnings(test_better_pull_review_review_queue_core_tests)

add_test(
    NAME core_review_queue
    COMMAND test_better_pull_review_review_queue_core_tests
)
