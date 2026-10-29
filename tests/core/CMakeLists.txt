# [[BPR-1674]] Built with the same configuration and hardening as the tests.
bpr_add_executable(test_qualification_assertion_probe ../support/qualification_assertion_probe.cpp)
bpr_enable_hardening(test_qualification_assertion_probe)
bpr_enable_warnings(test_qualification_assertion_probe)

bpr_add_executable(test_release_maintenance_core release_maintenance_tests.cpp)
target_link_libraries(test_release_maintenance_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_release_maintenance_core)
bpr_enable_warnings(test_release_maintenance_core)
bpr_add_network_isolated_test(NAME core_release_maintenance COMMAND test_release_maintenance_core)

bpr_add_executable(test_profile_data_reset_core profile_data_reset_tests.cpp)
target_link_libraries(test_profile_data_reset_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_profile_data_reset_core)
bpr_enable_warnings(test_profile_data_reset_core)
bpr_add_network_isolated_test(NAME core_profile_data_reset COMMAND test_profile_data_reset_core)

bpr_add_executable(test_release_update_core release_update_tests.cpp)
target_link_libraries(test_release_update_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_release_update_core)
bpr_enable_warnings(test_release_update_core)
bpr_add_network_isolated_test(NAME core_release_update COMMAND test_release_update_core)

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

# [[BPR-1557]] The Updates line's places, stops and steps, the base first.
bpr_add_executable(
    test_better_pull_review_update_line_core_tests
    update_line_tests.cpp
)

target_link_libraries(
    test_better_pull_review_update_line_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_update_line_core_tests)
bpr_enable_warnings(test_better_pull_review_update_line_core_tests)

bpr_add_network_isolated_test(
    NAME core_update_line
    COMMAND test_better_pull_review_update_line_core_tests
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

# [[BPR-1481]] Intraline ranges from dtl's common scalars.
bpr_add_executable(
    test_better_pull_review_diff_intraline_core_tests
    diff_intraline_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_intraline_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_intraline_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_intraline_core_tests)

bpr_add_network_isolated_test(
    NAME core_diff_intraline
    COMMAND test_better_pull_review_diff_intraline_core_tests
)

# [[BPR-1465]] The diff engine's evaluation seam.
bpr_add_executable(
    test_better_pull_review_diff_compute_components_core_tests
    diff_compute_components_tests.cpp
)

target_link_libraries(
    test_better_pull_review_diff_compute_components_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_diff_compute_components_core_tests)
bpr_enable_warnings(test_better_pull_review_diff_compute_components_core_tests)

bpr_add_network_isolated_test(
    NAME core_diff_compute_components
    COMMAND test_better_pull_review_diff_compute_components_core_tests
)

# [[BPR-1366]] Uninterrupted diff timings over large synthetic pairs. The row
# runs the smallest size as a check; run the executable with no arguments in an
# optimized tree for the timing table.
bpr_add_executable(
    bench_better_pull_review_diff_compute
    diff_compute_benchmark.cpp
)

target_link_libraries(
    bench_better_pull_review_diff_compute
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(bench_better_pull_review_diff_compute)
bpr_enable_warnings(bench_better_pull_review_diff_compute)

bpr_add_network_isolated_test(
    NAME core_diff_compute_benchmark
    COMMAND bench_better_pull_review_diff_compute 3 2000
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
    test_better_pull_review_comparison_preparation_core_tests
    comparison_preparation_tests.cpp
)

target_link_libraries(
    test_better_pull_review_comparison_preparation_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_comparison_preparation_core_tests)
bpr_enable_warnings(test_better_pull_review_comparison_preparation_core_tests)

bpr_add_network_isolated_test(
    NAME core_comparison_preparation
    COMMAND test_better_pull_review_comparison_preparation_core_tests
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

# [[BPR-1583]]. The one avatar rule: slot, initials and the tone table's contrast.
bpr_add_executable(
    test_better_pull_review_identity_avatar_core_tests
    identity_avatar_tests.cpp
)

target_link_libraries(
    test_better_pull_review_identity_avatar_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_identity_avatar_core_tests)
bpr_enable_warnings(test_better_pull_review_identity_avatar_core_tests)

bpr_add_network_isolated_test(
    NAME core_identity_avatar
    COMMAND test_better_pull_review_identity_avatar_core_tests
)
set_tests_properties(core_identity_avatar PROPERTIES PASS_REGULAR_EXPRESSION "identity-avatar-core-tests:pass")

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
    test_better_pull_review_conversation_reactions_core_tests
    conversation_reactions_tests.cpp
)

target_link_libraries(
    test_better_pull_review_conversation_reactions_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_conversation_reactions_core_tests)
bpr_enable_warnings(test_better_pull_review_conversation_reactions_core_tests)

bpr_add_network_isolated_test(
    NAME core_conversation_reactions
    COMMAND test_better_pull_review_conversation_reactions_core_tests
)

# [[BPR-1781]]. What Overview's Tour tab shows.
bpr_add_executable(
    test_better_pull_review_overview_tour_core_tests
    overview_tour_tests.cpp
)

target_link_libraries(
    test_better_pull_review_overview_tour_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_overview_tour_core_tests)
bpr_enable_warnings(test_better_pull_review_overview_tour_core_tests)

bpr_add_network_isolated_test(
    NAME core_overview_tour
    COMMAND test_better_pull_review_overview_tour_core_tests
)

# [[BPR-1784]]. The tour's three steps and where each stands.
bpr_add_executable(
    test_better_pull_review_tour_steps_core_tests
    tour_steps_tests.cpp
)

target_link_libraries(
    test_better_pull_review_tour_steps_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_tour_steps_core_tests)
bpr_enable_warnings(test_better_pull_review_tour_steps_core_tests)

bpr_add_network_isolated_test(
    NAME core_tour_steps
    COMMAND test_better_pull_review_tour_steps_core_tests
)

# [[BPR-1775]]. Who may edit a published comment and whether a save is written.
bpr_add_executable(
    test_better_pull_review_comment_edit_core_tests
    comment_edit_tests.cpp
)

target_link_libraries(
    test_better_pull_review_comment_edit_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_comment_edit_core_tests)
bpr_enable_warnings(test_better_pull_review_comment_edit_core_tests)

bpr_add_network_isolated_test(
    NAME core_comment_edit
    COMMAND test_better_pull_review_comment_edit_core_tests
)

# [[BPR-1774]]. Who may edit the description and whether a save is written.
bpr_add_executable(
    test_better_pull_review_description_edit_core_tests
    description_edit_tests.cpp
)

target_link_libraries(
    test_better_pull_review_description_edit_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_description_edit_core_tests)
bpr_enable_warnings(test_better_pull_review_description_edit_core_tests)

bpr_add_network_isolated_test(
    NAME core_description_edit
    COMMAND test_better_pull_review_description_edit_core_tests
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
    test_better_pull_review_home_ignore_core_tests
    home_ignore_tests.cpp
)

target_link_libraries(
    test_better_pull_review_home_ignore_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_home_ignore_core_tests)
bpr_enable_warnings(test_better_pull_review_home_ignore_core_tests)

bpr_add_network_isolated_test(
    NAME core_home_ignore
    COMMAND test_better_pull_review_home_ignore_core_tests
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

# [[BPR-1301]]. The Claude process wrapper path's text rules.
bpr_add_executable(test_better_pull_review_agent_wrapper_core_tests agent_wrapper_tests.cpp)
target_link_libraries(test_better_pull_review_agent_wrapper_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_agent_wrapper_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_wrapper_core_tests)
bpr_add_network_isolated_test(NAME core_agent_wrapper COMMAND test_better_pull_review_agent_wrapper_core_tests)
set_tests_properties(core_agent_wrapper PROPERTIES PASS_REGULAR_EXPRESSION "agent-wrapper-tests:pass")

# [[BPR-1748]]. RFC-BPR-0092's agent setup: rows, choice, checks, steps.
bpr_add_executable(test_better_pull_review_agent_setup_core_tests agent_setup_tests.cpp)
target_link_libraries(test_better_pull_review_agent_setup_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_agent_setup_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_setup_core_tests)
bpr_add_network_isolated_test(NAME core_agent_setup COMMAND test_better_pull_review_agent_setup_core_tests)
set_tests_properties(core_agent_setup PROPERTIES PASS_REGULAR_EXPRESSION "agent-setup-tests:pass")

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
    test_better_pull_review_agent_chat_scope_core_tests
    agent_chat_scope_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_chat_scope_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_chat_scope_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_chat_scope_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_chat_scope
    COMMAND test_better_pull_review_agent_chat_scope_core_tests
)
set_tests_properties(
    core_agent_chat_scope
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-chat-scope-tests:pass"
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
    test_better_pull_review_agent_finding_core_tests
    agent_finding_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_finding_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_finding_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_finding_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_finding
    COMMAND test_better_pull_review_agent_finding_core_tests
)
set_tests_properties(
    core_agent_finding
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-finding-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_finding_promotion_core_tests
    agent_finding_promotion_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_finding_promotion_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_agent_finding_promotion_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_finding_promotion_core_tests)

bpr_add_network_isolated_test(
    NAME core_agent_finding_promotion
    COMMAND test_better_pull_review_agent_finding_promotion_core_tests
)
set_tests_properties(
    core_agent_finding_promotion
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-finding-promotion-tests:pass"
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

bpr_add_executable(
    test_better_pull_review_anonymous_read_policy_core_tests
    anonymous_read_policy_tests.cpp
)

target_link_libraries(
    test_better_pull_review_anonymous_read_policy_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_anonymous_read_policy_core_tests)
bpr_enable_warnings(test_better_pull_review_anonymous_read_policy_core_tests)

bpr_add_network_isolated_test(
    NAME core_anonymous_public_read
    COMMAND test_better_pull_review_anonymous_read_policy_core_tests
)

bpr_add_executable(
    test_better_pull_review_review_submission_core_tests
    review_submission_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_submission_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_submission_core_tests)
bpr_enable_warnings(test_better_pull_review_review_submission_core_tests)
bpr_add_network_isolated_test(
    NAME core_review_submission
    COMMAND test_better_pull_review_review_submission_core_tests
)

# [[BPR-1680]]. Home discovery's diagnostics line and request planning.
bpr_add_executable(
    test_better_pull_review_home_discovery_core_tests
    home_discovery_tests.cpp
)
target_link_libraries(
    test_better_pull_review_home_discovery_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_home_discovery_core_tests)
bpr_enable_warnings(test_better_pull_review_home_discovery_core_tests)
bpr_add_network_isolated_test(
    NAME core_home_discovery
    COMMAND test_better_pull_review_home_discovery_core_tests
)

bpr_add_executable(
    test_better_pull_review_core_setup_destination_tests
    setup_destination_tests.cpp
)
target_link_libraries(test_better_pull_review_core_setup_destination_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_setup_destination_tests)
bpr_enable_warnings(test_better_pull_review_core_setup_destination_tests)
bpr_add_network_isolated_test(
    NAME core_setup_destination
    COMMAND test_better_pull_review_core_setup_destination_tests
)

# [[BPR-1358]]. SHA-256 known answers, padding and block boundaries, and
# every split of one input across update calls.
bpr_add_executable(
    test_better_pull_review_core_sha256_tests
    sha256_tests.cpp
)
target_link_libraries(test_better_pull_review_core_sha256_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_sha256_tests)
bpr_enable_warnings(test_better_pull_review_core_sha256_tests)

# Compile the exact CNG wrapper with a narrow API stub on every host. Native
# Windows correctness is a separate station observation, never inferred here.
add_library(bpr_test_sha256_cng_stub OBJECT
    sha256_cng_contract_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/core/src/sha256_hacl.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/src/sha256_cng.cpp"
)
target_include_directories(bpr_test_sha256_cng_stub PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cng_stub"
    "${PROJECT_SOURCE_DIR}/src/core/include"
)
target_link_libraries(bpr_test_sha256_cng_stub PRIVATE BetterPullReview::HaclSha2)
target_compile_features(bpr_test_sha256_cng_stub PRIVATE cxx_std_23)
bpr_enable_hardening(bpr_test_sha256_cng_stub)
bpr_enable_warnings(bpr_test_sha256_cng_stub)
# The injected quiet-CRT unit uses the real Windows SDK. Keep the narrow
# windows.h/bcrypt.h stubs private to the contract and wrapper compilation.
bpr_add_executable(test_better_pull_review_sha256_cng_contract
    $<TARGET_OBJECTS:bpr_test_sha256_cng_stub>
)
target_link_libraries(test_better_pull_review_sha256_cng_contract PRIVATE BetterPullReview::HaclSha2)
target_compile_features(test_better_pull_review_sha256_cng_contract PRIVATE cxx_std_23)
bpr_enable_hardening(test_better_pull_review_sha256_cng_contract)
bpr_enable_warnings(test_better_pull_review_sha256_cng_contract)
foreach(mode IN ITEMS success create duplicate update finish)
    bpr_add_network_isolated_test(NAME core_sha256_cng_${mode}
        COMMAND test_better_pull_review_sha256_cng_contract ${mode})
endforeach()
bpr_add_network_isolated_test(
    NAME core_sha256
    COMMAND test_better_pull_review_core_sha256_tests
)
set_tests_properties(core_sha256 PROPERTIES PASS_REGULAR_EXPRESSION "core-sha256-tests:pass")

bpr_add_executable(
    test_better_pull_review_core_pull_request_refusal_tests
    pull_request_refusal_tests.cpp
)
target_link_libraries(test_better_pull_review_core_pull_request_refusal_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_pull_request_refusal_tests)
bpr_enable_warnings(test_better_pull_review_core_pull_request_refusal_tests)
bpr_add_network_isolated_test(
    NAME core_pull_request_refusal
    COMMAND test_better_pull_review_core_pull_request_refusal_tests
)

bpr_add_executable(
    test_better_pull_review_core_profile_window_tests
    profile_window_tests.cpp
)
target_link_libraries(test_better_pull_review_core_profile_window_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_profile_window_tests)
bpr_enable_warnings(test_better_pull_review_core_profile_window_tests)
bpr_add_network_isolated_test(
    NAME core_profile_window
    COMMAND test_better_pull_review_core_profile_window_tests
)

bpr_add_executable(test_better_pull_review_github_window_read_tests github_window_read_tests.cpp)
target_link_libraries(test_better_pull_review_github_window_read_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_github_window_read_tests)
bpr_enable_warnings(test_better_pull_review_github_window_read_tests)
bpr_add_network_isolated_test(NAME core_github_window_read COMMAND test_better_pull_review_github_window_read_tests)

bpr_add_executable(test_better_pull_review_revisioned_review_draft_tests revisioned_review_draft_tests.cpp)
target_link_libraries(test_better_pull_review_revisioned_review_draft_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_revisioned_review_draft_tests)
bpr_enable_warnings(test_better_pull_review_revisioned_review_draft_tests)
bpr_add_network_isolated_test(NAME core_revisioned_review_draft COMMAND test_better_pull_review_revisioned_review_draft_tests)

bpr_add_executable(test_better_pull_review_window_review_restoration_core_tests window_review_restoration_tests.cpp)
target_link_libraries(test_better_pull_review_window_review_restoration_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_window_review_restoration_core_tests)
bpr_enable_warnings(test_better_pull_review_window_review_restoration_core_tests)
bpr_add_network_isolated_test(NAME core_window_review_restoration COMMAND test_better_pull_review_window_review_restoration_core_tests)
set_tests_properties(core_window_review_restoration PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_document_tabs_core_tests document_tabs_tests.cpp)
target_link_libraries(test_better_pull_review_document_tabs_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_document_tabs_core_tests)
bpr_enable_warnings(test_better_pull_review_document_tabs_core_tests)
bpr_add_network_isolated_test(NAME core_document_tabs COMMAND test_better_pull_review_document_tabs_core_tests)
set_tests_properties(core_document_tabs PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_markdown_navigation_tests markdown_navigation_tests.cpp)
target_link_libraries(test_better_pull_review_markdown_navigation_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_markdown_navigation_tests)
bpr_enable_warnings(test_better_pull_review_markdown_navigation_tests)
bpr_add_network_isolated_test(NAME core_markdown_navigation COMMAND test_better_pull_review_markdown_navigation_tests)
bpr_add_executable(test_better_pull_review_markdown_blob_snippet_tests markdown_blob_snippet_tests.cpp)
target_link_libraries(test_better_pull_review_markdown_blob_snippet_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_markdown_blob_snippet_tests)
bpr_enable_warnings(test_better_pull_review_markdown_blob_snippet_tests)
bpr_add_network_isolated_test(NAME core_markdown_blob_snippet COMMAND test_better_pull_review_markdown_blob_snippet_tests)
bpr_add_executable(test_better_pull_review_markdown_image_tests markdown_image_tests.cpp)
target_link_libraries(test_better_pull_review_markdown_image_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_markdown_image_tests)
bpr_enable_warnings(test_better_pull_review_markdown_image_tests)
bpr_add_network_isolated_test(NAME core_markdown_image COMMAND test_better_pull_review_markdown_image_tests)

bpr_add_executable(test_github_read_backoff github_read_backoff_tests.cpp)
target_link_libraries(test_github_read_backoff PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_github_read_backoff)
bpr_enable_warnings(test_github_read_backoff)
bpr_add_network_isolated_test(NAME core_github_read_backoff COMMAND test_github_read_backoff)

bpr_add_executable(test_review_tour_core review_tour_tests.cpp)
target_link_libraries(test_review_tour_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_tour_core)
bpr_enable_warnings(test_review_tour_core)
bpr_add_network_isolated_test(NAME core_review_tour COMMAND test_review_tour_core)

bpr_add_executable(test_review_tour_markdown review_tour_markdown_tests.cpp)
target_link_libraries(test_review_tour_markdown PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_tour_markdown)
bpr_enable_warnings(test_review_tour_markdown)
bpr_add_network_isolated_test(NAME core_review_tour_markdown COMMAND test_review_tour_markdown)

# [[BPR-1076]]. Conventional Comments catalog and bounded prefix recognizer, against the pack's catalog and corpus.
bpr_add_executable(test_feedback_convention feedback_convention_tests.cpp)
target_link_libraries(test_feedback_convention PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_convention)
bpr_enable_warnings(test_feedback_convention)
bpr_add_network_isolated_test(NAME core_feedback_convention COMMAND test_feedback_convention "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1077]]. Revisioned Conventional Comments header edits, against the pack's header-edit cases.
bpr_add_executable(test_feedback_header_edit feedback_header_edit_tests.cpp)
target_link_libraries(test_feedback_header_edit PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_header_edit)
bpr_enable_warnings(test_feedback_header_edit)
bpr_add_network_isolated_test(NAME core_feedback_header_edit COMMAND test_feedback_header_edit "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1082]]. Header edits as commands of the existing revisioned comment draft and tour-stop owners.
bpr_add_executable(test_feedback_draft_edit feedback_draft_edit_tests.cpp)
target_link_libraries(test_feedback_draft_edit PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_draft_edit)
bpr_enable_warnings(test_feedback_draft_edit)
bpr_add_network_isolated_test(NAME core_feedback_draft_edit COMMAND test_feedback_draft_edit)

# [[BPR-1084]]. Every label and prefix edit keeps a native suggested change byte for byte, through all four entry points.
bpr_add_executable(test_feedback_suggestion_preservation feedback_suggestion_preservation_tests.cpp)
target_link_libraries(test_feedback_suggestion_preservation PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_suggestion_preservation)
bpr_enable_warnings(test_feedback_suggestion_preservation)
bpr_add_network_isolated_test(NAME core_feedback_suggestion_preservation COMMAND test_feedback_suggestion_preservation "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1084]]. Native or text-only suggested changes, decided before Publish from the publication policy's own facts.
bpr_add_executable(test_suggestion_applicability suggestion_applicability_tests.cpp)
target_link_libraries(test_suggestion_applicability PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_suggestion_applicability)
bpr_enable_warnings(test_suggestion_applicability)
bpr_add_network_isolated_test(NAME core_suggestion_applicability COMMAND test_suggestion_applicability "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1212]]. Suggest change: the first text of a suggestion draft and what its block changes.
bpr_add_executable(test_suggestion_draft suggestion_draft_tests.cpp)
target_link_libraries(test_suggestion_draft PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_suggestion_draft)
bpr_enable_warnings(test_suggestion_draft)
bpr_add_network_isolated_test(NAME core_suggestion_draft COMMAND test_suggestion_draft)

# [[BPR-1077]]. Root-post classification and the scoped derived annotation set, against the pack's counter units.
bpr_add_executable(test_feedback_conversation feedback_conversation_tests.cpp)
target_link_libraries(test_feedback_conversation PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_conversation)
bpr_enable_warnings(test_feedback_conversation)
bpr_add_network_isolated_test(NAME core_feedback_conversation COMMAND test_feedback_conversation "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1078]]. Conversation partition, resolution states and count coverage, against the pack's counter cases and every label subset.
bpr_add_executable(test_feedback_partition feedback_partition_tests.cpp)
target_link_libraries(test_feedback_partition PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_partition)
bpr_enable_warnings(test_feedback_partition)
bpr_add_network_isolated_test(NAME core_feedback_partition COMMAND test_feedback_partition "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1")

# [[BPR-1088]]. The Overview label columns as a stored preference: all twelve in any order, repeats and unknown IDs refused or left out and counted.
bpr_add_executable(test_feedback_label_columns feedback_label_columns_tests.cpp)
target_link_libraries(test_feedback_label_columns PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_label_columns)
bpr_enable_warnings(test_feedback_label_columns)
bpr_add_network_isolated_test(NAME core_feedback_label_columns COMMAND test_feedback_label_columns)

# [[BPR-1091]]. What a comment-workshop candidate changes besides wording: label (shared recognizer), question, suggestion bytes.
bpr_add_executable(test_feedback_workshop_candidate feedback_workshop_candidate_tests.cpp)
target_link_libraries(test_feedback_workshop_candidate PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_workshop_candidate)
bpr_enable_warnings(test_feedback_workshop_candidate)
bpr_add_network_isolated_test(NAME core_feedback_workshop_candidate COMMAND test_feedback_workshop_candidate)
set_tests_properties(core_feedback_workshop_candidate PROPERTIES PASS_REGULAR_EXPRESSION "feedback-workshop-candidate-tests:pass")

# [[BPR-1086]]. Private promotion into the shared draft keeps lineage and leaks no private severity, category or provenance.
bpr_add_executable(test_feedback_private_promotion feedback_private_promotion_tests.cpp)
target_link_libraries(test_feedback_private_promotion PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_private_promotion)
bpr_enable_warnings(test_feedback_private_promotion)
bpr_add_network_isolated_test(NAME core_feedback_private_promotion COMMAND test_feedback_private_promotion)

# [[BPR-1086]]. A tour's plain and labelled stops, index links and ordinary threads through one segment owner and the shared population.
bpr_add_executable(test_feedback_tour_coexistence feedback_tour_coexistence_tests.cpp)
target_link_libraries(test_feedback_tour_coexistence PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_feedback_tour_coexistence)
bpr_enable_warnings(test_feedback_tour_coexistence)
bpr_add_network_isolated_test(NAME core_feedback_tour_coexistence COMMAND test_feedback_tour_coexistence)

# [[BPR-1085]]. The submitting reviewer's own feedback before a native review decision, and which changes invalidate a prepared decision.
bpr_add_executable(test_review_preflight review_preflight_tests.cpp)
target_link_libraries(test_review_preflight PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_preflight)
bpr_enable_warnings(test_review_preflight)
bpr_add_network_isolated_test(NAME core_review_preflight COMMAND test_review_preflight)

bpr_add_executable(test_review_tour_import review_tour_import_tests.cpp)
target_link_libraries(test_review_tour_import PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_tour_import)
bpr_enable_warnings(test_review_tour_import)
bpr_add_network_isolated_test(NAME core_review_tour_import COMMAND test_review_tour_import)

bpr_add_executable(test_review_tour_destination review_tour_destination_tests.cpp)
target_link_libraries(test_review_tour_destination PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_tour_destination)
bpr_enable_warnings(test_review_tour_destination)
bpr_add_network_isolated_test(NAME core_review_tour_destination COMMAND test_review_tour_destination)

bpr_add_executable(test_review_tour_update review_tour_update_tests.cpp)
target_link_libraries(test_review_tour_update PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_tour_update)
bpr_enable_warnings(test_review_tour_update)
bpr_add_network_isolated_test(NAME core_review_tour_update COMMAND test_review_tour_update)

bpr_add_executable(test_commit_filter commit_filter_tests.cpp)
target_link_libraries(test_commit_filter PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_commit_filter)
bpr_enable_warnings(test_commit_filter)
bpr_add_network_isolated_test(NAME core_commit_filter COMMAND test_commit_filter)

bpr_add_executable(test_generated_files generated_files_tests.cpp)
target_link_libraries(test_generated_files PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_generated_files)
bpr_enable_warnings(test_generated_files)
bpr_add_network_isolated_test(NAME core_generated_files COMMAND test_generated_files)

# RFC-BPR-0089 M4: where a comment on a commit-filtered diff goes, and which
# threads are marked on it (AC07, AC08).
bpr_add_executable(test_filtered_comment filtered_comment_tests.cpp)
target_link_libraries(test_filtered_comment PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_filtered_comment)
bpr_enable_warnings(test_filtered_comment)
bpr_add_network_isolated_test(NAME core_filtered_comment COMMAND test_filtered_comment)

# [[BPR-1736]]: the learned comment-anywhere fact, its lifetime and where a
# comment goes under it.
bpr_add_executable(test_off_hunk_fact off_hunk_fact_tests.cpp)
target_link_libraries(test_off_hunk_fact PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_off_hunk_fact)
bpr_enable_warnings(test_off_hunk_fact)
bpr_add_network_isolated_test(NAME core_off_hunk_fact COMMAND test_off_hunk_fact)

bpr_add_executable(test_review_preparation review_preparation_tests.cpp)
target_link_libraries(test_review_preparation PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_preparation)
bpr_enable_warnings(test_review_preparation)
bpr_add_network_isolated_test(NAME core_review_preparation COMMAND test_review_preparation)

bpr_add_executable(test_private_review_core private_review_tests.cpp)
target_link_libraries(test_private_review_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_private_review_core)
bpr_enable_warnings(test_private_review_core)
bpr_add_network_isolated_test(NAME core_private_review COMMAND test_private_review_core)

bpr_add_executable(test_review_composition_core review_composition_tests.cpp)
target_link_libraries(test_review_composition_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_composition_core)
bpr_enable_warnings(test_review_composition_core)
bpr_add_network_isolated_test(NAME core_review_composition COMMAND test_review_composition_core)

bpr_add_executable(test_better_pull_review_review_cycle_tests review_cycle_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_tests)
bpr_add_network_isolated_test(NAME core_review_cycle COMMAND test_better_pull_review_review_cycle_tests)

bpr_add_executable(test_better_pull_review_review_cycle_policy_tests review_cycle_policy_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_policy_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_policy_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_policy_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_policy COMMAND test_better_pull_review_review_cycle_policy_tests)

bpr_add_executable(test_better_pull_review_review_cycle_workspace_tests review_cycle_workspace_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_workspace_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_workspace_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_workspace_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_workspace COMMAND test_better_pull_review_review_cycle_workspace_tests)

bpr_add_executable(test_better_pull_review_review_cycle_material_tests review_cycle_material_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_material_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_material_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_material_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_material COMMAND test_better_pull_review_review_cycle_material_tests)

bpr_add_executable(test_better_pull_review_review_cycle_manifest_tests review_cycle_manifest_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_manifest_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_manifest_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_manifest_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_manifest COMMAND test_better_pull_review_review_cycle_manifest_tests)

bpr_add_executable(test_better_pull_review_review_cycle_comparison_tests review_cycle_comparison_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_comparison_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_comparison_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_comparison_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_comparison COMMAND test_better_pull_review_review_cycle_comparison_tests)

bpr_add_executable(test_better_pull_review_review_cycle_feedback_tests review_cycle_feedback_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_feedback_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_feedback_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_feedback_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_feedback COMMAND test_better_pull_review_review_cycle_feedback_tests)

bpr_add_executable(test_better_pull_review_review_cycle_suggestion_tests review_cycle_suggestion_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_suggestion_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_suggestion_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_suggestion_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_suggestion COMMAND test_better_pull_review_review_cycle_suggestion_tests)

bpr_add_executable(test_better_pull_review_review_cycle_disclosure_tests review_cycle_disclosure_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_disclosure_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_disclosure_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_disclosure_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_disclosure COMMAND test_better_pull_review_review_cycle_disclosure_tests)

bpr_add_executable(test_better_pull_review_suggestion_text_editing_tests suggestion_text_editing_tests.cpp)
target_link_libraries(test_better_pull_review_suggestion_text_editing_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_suggestion_text_editing_tests)
bpr_enable_warnings(test_better_pull_review_suggestion_text_editing_tests)
bpr_add_network_isolated_test(NAME core_suggestion_text_editing COMMAND test_better_pull_review_suggestion_text_editing_tests)

bpr_add_executable(test_better_pull_review_review_cycle_tabs_tests review_cycle_tabs_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_tabs_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_tabs_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_tabs_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_tabs COMMAND test_better_pull_review_review_cycle_tabs_tests)

# [[BPR-1163]]. Named comparison views, reviewer checkpoints, pinned tabs and revision chronologies.
bpr_add_executable(test_better_pull_review_review_cycle_views_tests review_cycle_views_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_views_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_views_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_views_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_views COMMAND test_better_pull_review_review_cycle_views_tests)

# [[BPR-1168]]. The finite cycle command registry and the owner's admission beside an open draft.
bpr_add_executable(test_better_pull_review_review_cycle_command_tests review_cycle_command_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_command_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_command_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_command_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_command COMMAND test_better_pull_review_review_cycle_command_tests)

# [[BPR-1169]]. Reading one frozen round from outside: measure, whole-item pages bound to the digest, attachments.
bpr_add_executable(test_better_pull_review_review_cycle_retrieval_tests review_cycle_retrieval_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_retrieval_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_retrieval_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_retrieval_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_retrieval COMMAND test_better_pull_review_review_cycle_retrieval_tests)

# [[BPR-1164]]. Recipient responses and manual replies: the pack's response, forged authority and correlation.
bpr_add_executable(test_better_pull_review_review_cycle_response_tests review_cycle_response_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_response_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_response_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_response_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_response COMMAND test_better_pull_review_review_cycle_response_tests "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")

# [[BPR-1165]]. Reconciliation, explicit verification bound to checked material, and next-round carry-forward.
bpr_add_executable(test_better_pull_review_review_cycle_verification_tests review_cycle_verification_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_verification_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_verification_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_verification_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_verification COMMAND test_better_pull_review_review_cycle_verification_tests)

# [[BPR-1172]]. Canonical Git blob, tree and commit identities in both object formats.
bpr_add_executable(test_better_pull_review_git_objects_tests git_objects_tests.cpp)
target_link_libraries(test_better_pull_review_git_objects_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_git_objects_tests)
bpr_enable_warnings(test_better_pull_review_git_objects_tests)
bpr_add_network_isolated_test(NAME core_git_objects COMMAND test_better_pull_review_git_objects_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/sha1-collisions")

# [[BPR-1172]]. Selected outgoing content over an explicit Git base: R0, Rn, P and T.
bpr_add_executable(test_better_pull_review_review_cycle_outgoing_tests review_cycle_outgoing_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_outgoing_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_outgoing_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_outgoing_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_outgoing COMMAND test_better_pull_review_review_cycle_outgoing_tests)

# [[BPR-1173]]. The squashed candidate: one parent, exact metadata, named normalization and limits.
bpr_add_executable(test_better_pull_review_review_cycle_candidate_tests review_cycle_candidate_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_candidate_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_candidate_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_candidate_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_candidate COMMAND test_better_pull_review_review_cycle_candidate_tests)

# [[BPR-1174]]. The finite publication plan, its authorization and the recovery journal.
bpr_add_executable(test_better_pull_review_review_cycle_publication_tests review_cycle_publication_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_publication_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_publication_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_publication_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_publication COMMAND test_better_pull_review_review_cycle_publication_tests)

# [[BPR-1177]]. A cycle's pull request association: addresses, external admission, encoding.
bpr_add_executable(test_better_pull_review_review_cycle_association_tests review_cycle_association_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_association_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_association_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_association_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_association COMMAND test_better_pull_review_review_cycle_association_tests)
# [[BPR-1273]]. Local notes promoted into the linked pull request's Review Tour.
bpr_add_executable(test_better_pull_review_review_cycle_promotion_tests review_cycle_promotion_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_promotion_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_promotion_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_promotion_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_promotion COMMAND test_better_pull_review_review_cycle_promotion_tests)

# [[BPR-1158]]. Canonical round payloads, the pack's golden round and assembly.
bpr_add_executable(test_better_pull_review_review_cycle_round_tests review_cycle_round_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_round_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_round_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_round_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_round COMMAND test_better_pull_review_review_cycle_round_tests "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")

# [[BPR-1159]]. The readable round document: the pack's review.md, hostile Markdown and budgets.
bpr_add_executable(test_better_pull_review_review_cycle_round_markdown_tests review_cycle_round_markdown_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_round_markdown_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_round_markdown_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_round_markdown_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_round_markdown COMMAND test_better_pull_review_review_cycle_round_markdown_tests "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")

# [[BPR-1161]]. Share routes decided as authorize decides, Compose feedback's edits and delivery observations.
bpr_add_executable(test_better_pull_review_review_cycle_share_tests review_cycle_share_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_share_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_share_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_share_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_share COMMAND test_better_pull_review_review_cycle_share_tests)

# [[BPR-1160]]. Portable review bundles: the pack's bundle, determinism and the adversarial corpus.
bpr_add_executable(test_better_pull_review_review_cycle_bundle_tests review_cycle_bundle_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_bundle_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_bundle_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_bundle_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_bundle COMMAND test_better_pull_review_review_cycle_bundle_tests "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")
if(TARGET Python3::Interpreter)
    bpr_add_network_isolated_test(
        NAME core_review_cycle_bundle_pack_oracle
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/round_bundle_oracle_tests.py"
            "$<TARGET_FILE:test_better_pull_review_review_cycle_bundle_tests>"
            "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1"
    )
endif()

# [[BPR-1162]]. Attaching imported rounds through explicit project and revision mappings.
bpr_add_executable(test_better_pull_review_review_cycle_import_tests review_cycle_import_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_import_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_import_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_import_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_import COMMAND test_better_pull_review_review_cycle_import_tests "${PROJECT_SOURCE_DIR}/docs/rfcs/assets/bpr-review-cycles-v1/examples")

# [[BPR-1180]]. Send to Agent's decision, sessions and observations.
bpr_add_executable(test_better_pull_review_review_cycle_send_tests review_cycle_send_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_send_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_review_cycle_send_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_send_tests)
bpr_add_network_isolated_test(NAME core_review_cycle_send COMMAND test_better_pull_review_review_cycle_send_tests)

# [[BPR-1289]]. The ranked fuzzy matcher for Settings search.
bpr_add_executable(test_better_pull_review_fuzzy_match_tests fuzzy_match_tests.cpp)
target_link_libraries(test_better_pull_review_fuzzy_match_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_fuzzy_match_tests)
bpr_enable_warnings(test_better_pull_review_fuzzy_match_tests)
bpr_add_network_isolated_test(NAME core_fuzzy_match COMMAND test_better_pull_review_fuzzy_match_tests)

# [[BPR-1298]]. A body's line endings kept through the desktop editors.
bpr_add_executable(test_better_pull_review_line_endings_tests line_endings_tests.cpp)
target_link_libraries(test_better_pull_review_line_endings_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_line_endings_tests)
bpr_enable_warnings(test_better_pull_review_line_endings_tests)
bpr_add_network_isolated_test(NAME core_line_endings COMMAND test_better_pull_review_line_endings_tests)

# [[BPR-1212]]. A new comment on one line or several: the range GitHub takes,
# its typed refusals, the draft and proposal that carry it, and the suggestion
# a selection starts.
bpr_add_executable(test_review_comment_range review_comment_range_tests.cpp)
target_link_libraries(test_review_comment_range PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_comment_range)
bpr_enable_warnings(test_review_comment_range)
bpr_add_network_isolated_test(NAME core_review_comment_range COMMAND test_review_comment_range)

# [[RFC-BPR-0090]] M1. Suggest change on the selected head lines: the range it
# opens on, the editable span, the plan GitHub receives, the composed body,
# rebuilding a kept draft, Convert's text and carrying to a newer head.
bpr_add_executable(test_pull_request_suggestion pull_request_suggestion_tests.cpp)
target_link_libraries(test_pull_request_suggestion PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_pull_request_suggestion)
bpr_enable_warnings(test_pull_request_suggestion)
bpr_add_network_isolated_test(NAME core_pull_request_suggestion COMMAND test_pull_request_suggestion)

# [[BPR-1345]]. Start a review: the pending mark, the lock a submitted review
# holds, and the review that carries pending drafts as its line comments.
bpr_add_executable(test_review_pending review_pending_tests.cpp)
target_link_libraries(test_review_pending PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_review_pending)
bpr_enable_warnings(test_review_pending)
bpr_add_network_isolated_test(NAME core_review_pending COMMAND test_review_pending)

bpr_add_executable(test_better_pull_review_core_home_open_tests home_open_tests.cpp)
target_link_libraries(test_better_pull_review_core_home_open_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_home_open_tests)
bpr_enable_warnings(test_better_pull_review_core_home_open_tests)
bpr_add_network_isolated_test(NAME core_home_open COMMAND test_better_pull_review_core_home_open_tests)

# [[BPR-1576]]. The application's one loading-state pattern.
bpr_add_executable(test_better_pull_review_core_pending_presentation_tests pending_presentation_tests.cpp)
target_link_libraries(test_better_pull_review_core_pending_presentation_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_pending_presentation_tests)
bpr_enable_warnings(test_better_pull_review_core_pending_presentation_tests)
bpr_add_network_isolated_test(NAME core_pending_presentation COMMAND test_better_pull_review_core_pending_presentation_tests)

# [[BPR-1575]]. A refresh's lifecycle, changes, age words and announcements.
bpr_add_executable(test_better_pull_review_core_refresh_feedback_tests refresh_feedback_tests.cpp)
target_link_libraries(test_better_pull_review_core_refresh_feedback_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_core_refresh_feedback_tests)
bpr_enable_warnings(test_better_pull_review_core_refresh_feedback_tests)
bpr_add_network_isolated_test(NAME core_refresh_feedback COMMAND test_better_pull_review_core_refresh_feedback_tests)

# [[BPR-1374]]. An unsent reply's standing against the server's threads: kept
# on its thread through a Refresh and a restart, flagged when its thread is
# resolved or deleted elsewhere, never sent to a thread that is gone.
bpr_add_executable(test_reply_draft_standing reply_draft_standing_tests.cpp)
target_link_libraries(test_reply_draft_standing PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_reply_draft_standing)
bpr_enable_warnings(test_reply_draft_standing)
bpr_add_network_isolated_test(NAME core_reply_draft_standing COMMAND test_reply_draft_standing)
set_tests_properties(core_reply_draft_standing PROPERTIES PASS_REGULAR_EXPRESSION "REPLY_DRAFT_STANDING_OK")

# [[BPR-1745]]. An outdated thread takes a reply bound to the thread, as
# GitHub's web interface does; GitHub's thread data decides whether it may.
bpr_add_executable(test_thread_reply thread_reply_tests.cpp)
target_link_libraries(test_thread_reply PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_thread_reply)
bpr_enable_warnings(test_thread_reply)
bpr_add_network_isolated_test(NAME core_thread_reply COMMAND test_thread_reply)
set_tests_properties(core_thread_reply PROPERTIES PASS_REGULAR_EXPRESSION "THREAD_REPLY_OK")

# [[BPR-1591]]. A private note's line keeps or is outdated, never moved.
bpr_add_executable(test_note_placement note_placement_tests.cpp)
target_link_libraries(test_note_placement PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_note_placement)
bpr_enable_warnings(test_note_placement)
bpr_add_network_isolated_test(NAME core_note_placement COMMAND test_note_placement)
set_tests_properties(core_note_placement PROPERTIES PASS_REGULAR_EXPRESSION "NOTE_PLACEMENT_OK")

bpr_add_executable(test_remembered_connection_policy remembered_connection_tests.cpp)
target_link_libraries(test_remembered_connection_policy PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_remembered_connection_policy)
bpr_enable_warnings(test_remembered_connection_policy)
bpr_add_network_isolated_test(NAME core_remembered_connection_policy COMMAND test_remembered_connection_policy)

bpr_add_executable(test_better_pull_review_header_source_navigation_core_tests header_source_navigation_tests.cpp)
target_link_libraries(test_better_pull_review_header_source_navigation_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_header_source_navigation_core_tests)
bpr_enable_warnings(test_better_pull_review_header_source_navigation_core_tests)
bpr_add_network_isolated_test(NAME core_header_source_navigation COMMAND test_better_pull_review_header_source_navigation_core_tests)

bpr_add_executable(test_source_search_policy source_search_tests.cpp)
target_link_libraries(test_source_search_policy PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_source_search_policy)
bpr_enable_warnings(test_source_search_policy)
bpr_add_network_isolated_test(NAME core_source_search_policy COMMAND test_source_search_policy)

# [[BPR-1618]]. The production matcher also runs in Qt-free core builds.
bpr_add_executable(test_source_matcher source_matcher_tests.cpp)
target_link_libraries(test_source_matcher PRIVATE BetterPullReview::Core Threads::Threads)
bpr_enable_hardening(test_source_matcher)
bpr_enable_warnings(test_source_matcher)
bpr_add_network_isolated_test(NAME core_source_matcher COMMAND test_source_matcher)
bpr_add_network_isolated_test(
    NAME core_source_matcher_reference
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/source_matcher_reference_tests.py" "$<TARGET_FILE:test_source_matcher>"
)

# [[BPR-1433]]. The one structural JSON check every response decoder shares.
bpr_add_executable(test_json_preflight json_preflight_tests.cpp)
target_link_libraries(test_json_preflight PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_json_preflight)
bpr_enable_warnings(test_json_preflight)
bpr_add_network_isolated_test(NAME core_json_preflight COMMAND test_json_preflight)
# [[BPR-1562]], [[BPR-1563]], [[BPR-1564]]. Fenced code, against GitHub's own rendering.
bpr_add_executable(test_better_pull_review_markdown_code_tests markdown_code_tests.cpp)
target_link_libraries(test_better_pull_review_markdown_code_tests PRIVATE BetterPullReview::Core)
target_compile_definitions(test_better_pull_review_markdown_code_tests PRIVATE
    "BPR_MARKDOWN_CODE_FIXTURES=\"${PROJECT_SOURCE_DIR}/tests/fixtures/markdown/code-blocks-v1\"")
bpr_enable_hardening(test_better_pull_review_markdown_code_tests)
bpr_enable_warnings(test_better_pull_review_markdown_code_tests)
bpr_add_network_isolated_test(NAME core_markdown_code COMMAND test_better_pull_review_markdown_code_tests)

# [[BPR-1589]]. My role and the attention list.
bpr_add_executable(
    test_better_pull_review_review_role_core_tests
    review_role_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_role_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_review_role_core_tests)
bpr_enable_warnings(test_better_pull_review_review_role_core_tests)

bpr_add_network_isolated_test(
    NAME core_review_role
    COMMAND test_better_pull_review_review_role_core_tests
)

bpr_add_executable(test_insights_core insights_tests.cpp)
target_link_libraries(test_insights_core PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_insights_core)
bpr_enable_warnings(test_insights_core)
bpr_add_network_isolated_test(NAME core_insights COMMAND test_insights_core)

# BPR-1619: public computation factories need only core and the xdiff DSO.
bpr_add_executable(test_core_computation_boundaries core_computation_boundaries_tests.cpp)
# Exclude the directory-wide host-backed product-diff helper: this course
# deliberately verifies the core library and DSO without application linkage.
set_property(TARGET test_core_computation_boundaries PROPERTY LINK_LIBRARIES BetterPullReview::Core)
bpr_enable_hardening(test_core_computation_boundaries)
bpr_enable_warnings(test_core_computation_boundaries)
bpr_add_network_isolated_test(NAME core_computation_boundaries COMMAND test_core_computation_boundaries)

# [[BPR-1750]]. RFC-BPR-0092 D2: clone remotes, URLs and matching.
bpr_add_executable(test_better_pull_review_clone_discovery_core_tests clone_discovery_tests.cpp)
target_link_libraries(test_better_pull_review_clone_discovery_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_clone_discovery_core_tests)
bpr_enable_warnings(test_better_pull_review_clone_discovery_core_tests)
bpr_add_network_isolated_test(NAME core_clone_discovery COMMAND test_better_pull_review_clone_discovery_core_tests)
set_tests_properties(core_clone_discovery PROPERTIES PASS_REGULAR_EXPRESSION "clone-discovery-tests:pass")

# [[BPR-1752]]. RFC-BPR-0092 D1 and Q9 A: the review skill's front matter,
# the newer-version offer and the review brief's bounds.
bpr_add_executable(test_better_pull_review_review_skill_core_tests review_skill_tests.cpp)
target_link_libraries(test_better_pull_review_review_skill_core_tests PRIVATE BetterPullReview::Core)
target_compile_definitions(test_better_pull_review_review_skill_core_tests PRIVATE
    BPR_TEST_REVIEW_SKILL="${PROJECT_SOURCE_DIR}/packaging/agent-skills/bpr-pr-review/SKILL.md")
bpr_enable_hardening(test_better_pull_review_review_skill_core_tests)
bpr_enable_warnings(test_better_pull_review_review_skill_core_tests)
bpr_add_network_isolated_test(NAME core_review_skill COMMAND test_better_pull_review_review_skill_core_tests)
set_tests_properties(core_review_skill PROPERTIES PASS_REGULAR_EXPRESSION "review-skill-tests:pass")

# [[BPR-1753]]. RFC-BPR-0092 owner amendment 2026-10-23: the agent list,
# its slots, its edits and the move from the single Claude and Codex settings.
bpr_add_executable(test_better_pull_review_agent_entry_core_tests agent_entry_tests.cpp)
target_link_libraries(test_better_pull_review_agent_entry_core_tests PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_better_pull_review_agent_entry_core_tests)
bpr_enable_warnings(test_better_pull_review_agent_entry_core_tests)
bpr_add_network_isolated_test(NAME core_agent_entry COMMAND test_better_pull_review_agent_entry_core_tests)
set_tests_properties(core_agent_entry PROPERTIES PASS_REGULAR_EXPRESSION "agent-entry-tests:pass")

# [[BPR-1779]]. The conversation views list comments; a review is the bundle
# they were posted in, and only a verdict that matters adds a row of its own.
bpr_add_executable(test_conversation_timeline conversation_timeline_tests.cpp)
target_link_libraries(test_conversation_timeline PRIVATE BetterPullReview::Core)
bpr_enable_hardening(test_conversation_timeline)
bpr_enable_warnings(test_conversation_timeline)
bpr_add_network_isolated_test(NAME core_conversation_timeline COMMAND test_conversation_timeline)
set_tests_properties(core_conversation_timeline PROPERTIES PASS_REGULAR_EXPRESSION "CONVERSATION_TIMELINE_OK")
