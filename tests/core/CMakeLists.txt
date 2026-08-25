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

bpr_add_network_isolated_test(NAME core_foundation COMMAND test_better_pull_review_core_tests)

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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
    NAME core_review_queue
    COMMAND test_better_pull_review_review_queue_core_tests
)

bpr_add_executable(
    test_better_pull_review_review_queue_attention_core_tests
    review_queue_attention_tests.cpp
)

target_link_libraries(
    test_better_pull_review_review_queue_attention_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_queue_attention_core_tests)
bpr_enable_warnings(test_better_pull_review_review_queue_attention_core_tests)

bpr_add_network_isolated_test(
    NAME core_review_queue_attention
    COMMAND test_better_pull_review_review_queue_attention_core_tests
)

bpr_add_executable(
    test_better_pull_review_agent_profile_core_tests
    agent_profile_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_profile_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_profile_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_profile_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_profile
    COMMAND test_better_pull_review_agent_profile_core_tests
)
set_tests_properties(
    core_agent_profile
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-profile-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_session_core_tests
    agent_session_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_session_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_session_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_session_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_session
    COMMAND test_better_pull_review_agent_session_core_tests
)
set_tests_properties(
    core_agent_session
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-session-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_context_supplement_core_tests
    agent_context_supplement_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_context_supplement_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_context_supplement_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_context_supplement_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_context_supplement
    COMMAND test_better_pull_review_agent_context_supplement_core_tests
)
set_tests_properties(
    core_agent_context_supplement
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-context-supplement-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_device_authorization_core_tests
    device_authorization_tests.cpp
)

target_link_libraries(
    test_better_pull_review_device_authorization_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_device_authorization_core_tests)
bpr_enable_warnings(test_better_pull_review_device_authorization_core_tests)

bpr_add_network_isolated_test(
    NAME core_device_authorization
    COMMAND test_better_pull_review_device_authorization_core_tests
)
