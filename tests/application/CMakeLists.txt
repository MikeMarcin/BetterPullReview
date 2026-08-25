bpr_add_executable(
    test_better_pull_review_application_tests
    host_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_tests)
bpr_enable_warnings(test_better_pull_review_application_tests)

bpr_add_network_isolated_test(NAME application_host COMMAND test_better_pull_review_application_tests)

bpr_add_executable(
    test_better_pull_review_review_mutation_application_host_tests
    review_mutation_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_mutation_application_host_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_review_mutation_application_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_review_mutation_application_host_tests)
bpr_enable_warnings(test_better_pull_review_review_mutation_application_host_tests)

bpr_add_network_isolated_test(
    NAME application_review_mutation_application_host
    COMMAND test_better_pull_review_review_mutation_application_host_tests
)

bpr_add_executable(
    test_better_pull_review_designated_review_authority_bootstrap_tests
    designated_review_authority_bootstrap_tests.cpp
)
target_link_libraries(
    test_better_pull_review_designated_review_authority_bootstrap_tests
    PRIVATE
        BetterPullReview::Application
)
target_include_directories(
    test_better_pull_review_designated_review_authority_bootstrap_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/application/src"
)
target_compile_definitions(
    test_better_pull_review_designated_review_authority_bootstrap_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_designated_review_authority_bootstrap_tests
)
bpr_enable_warnings(
    test_better_pull_review_designated_review_authority_bootstrap_tests
)
bpr_add_network_isolated_test(
    NAME application_designated_review_authority_bootstrap
    COMMAND test_better_pull_review_designated_review_authority_bootstrap_tests
)
set_tests_properties(
    application_designated_review_authority_bootstrap
    PROPERTIES
        PASS_REGULAR_EXPRESSION
            "designated-review-authority-bootstrap-tests:(pass|platform-deferred)"
)
set_tests_properties(
    application_review_mutation_application_host
    PROPERTIES
        PASS_REGULAR_EXPRESSION "review-mutation-application-host-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_application_github_host_tests
    github_application_host_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_github_host_tests)
bpr_enable_warnings(test_better_pull_review_application_github_host_tests)

bpr_add_network_isolated_test(
    NAME application_github_host
    COMMAND test_better_pull_review_application_github_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_contract_tests
    github_contract_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_github_contract_tests)
bpr_enable_warnings(test_better_pull_review_application_github_contract_tests)

bpr_add_network_isolated_test(
    NAME application_github_contract
    COMMAND test_better_pull_review_application_github_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_capability_cache_tests
    github_capability_cache_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_capability_cache_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_github_capability_cache_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_github_capability_cache_tests
)

bpr_add_network_isolated_test(
    NAME application_github_capability_cache
    COMMAND test_better_pull_review_application_github_capability_cache_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_credential_vault_tests
    github_credential_vault_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_credential_vault_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_github_credential_vault_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_github_credential_vault_tests
)

bpr_add_network_isolated_test(
    NAME application_github_credential_vault
    COMMAND test_better_pull_review_application_github_credential_vault_tests
)

bpr_add_executable(
    test_better_pull_review_application_git_protocol_v2_tests
    git_protocol_v2_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_git_protocol_v2_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_git_protocol_v2_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_git_protocol_v2_tests
)

bpr_add_network_isolated_test(
    NAME application_git_protocol_v2
    COMMAND test_better_pull_review_application_git_protocol_v2_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_catalog_contract_tests
    epoch_catalog_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_catalog_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_catalog_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_catalog_contract_tests
)

bpr_add_network_isolated_test(
    NAME application_epoch_catalog_contract
    COMMAND test_better_pull_review_application_epoch_catalog_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_contract_tests
    epoch_application_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_contract_tests
)

bpr_add_network_isolated_test(
    NAME application_epoch_contract
    COMMAND test_better_pull_review_application_epoch_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_host_tests
    epoch_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_host_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_application_epoch_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_host_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_host_tests
)

bpr_add_network_isolated_test(
    NAME application_epoch_host
    COMMAND test_better_pull_review_application_epoch_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_diff_host_tests
    diff_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_diff_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_diff_host_tests)
bpr_enable_warnings(test_better_pull_review_application_diff_host_tests)

bpr_add_network_isolated_test(
    NAME application_diff_host
    COMMAND test_better_pull_review_application_diff_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_appearance_host_tests
    appearance_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_appearance_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_appearance_host_tests)
bpr_enable_warnings(test_better_pull_review_application_appearance_host_tests)

bpr_add_network_isolated_test(
    NAME application_appearance_host
    COMMAND test_better_pull_review_application_appearance_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_workbench_host_tests
    workbench_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_workbench_host_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_application_workbench_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_application_workbench_host_tests)
bpr_enable_warnings(test_better_pull_review_application_workbench_host_tests)

bpr_add_network_isolated_test(
    NAME application_workbench_host
    COMMAND test_better_pull_review_application_workbench_host_tests
)

bpr_add_executable(
    test_better_pull_review_review_ledger_host_tests
    review_ledger_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_ledger_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_ledger_host_tests)
bpr_enable_warnings(test_better_pull_review_review_ledger_host_tests)

bpr_add_network_isolated_test(
    NAME application_review_ledger_host
    COMMAND test_better_pull_review_review_ledger_host_tests
)

bpr_add_executable(
    test_better_pull_review_review_mutation_tests
    review_mutation_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_mutation_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_mutation_tests)
bpr_enable_warnings(test_better_pull_review_review_mutation_tests)

bpr_add_network_isolated_test(
    NAME application_review_mutation
    COMMAND test_better_pull_review_review_mutation_tests
)

bpr_add_executable(
    test_better_pull_review_review_synchronization_tests
    review_synchronization_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_synchronization_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_synchronization_tests)
bpr_enable_warnings(test_better_pull_review_review_synchronization_tests)

bpr_add_network_isolated_test(
    NAME application_review_synchronization
    COMMAND test_better_pull_review_review_synchronization_tests
)

bpr_add_executable(
    test_better_pull_review_defect_correlation_tests
    defect_correlation_tests.cpp
)
target_link_libraries(
    test_better_pull_review_defect_correlation_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_defect_correlation_tests)
bpr_enable_warnings(test_better_pull_review_defect_correlation_tests)

bpr_add_network_isolated_test(
    NAME application_defect_correlation
    COMMAND test_better_pull_review_defect_correlation_tests
)

bpr_add_executable(
    test_better_pull_review_review_queue_refresh_tests
    review_queue_refresh_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_queue_refresh_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_queue_refresh_tests)
bpr_enable_warnings(test_better_pull_review_review_queue_refresh_tests)

bpr_add_network_isolated_test(
    NAME application_review_queue_refresh
    COMMAND test_better_pull_review_review_queue_refresh_tests
)

bpr_add_executable(
    test_better_pull_review_review_queue_enrollment_tests
    review_queue_enrollment_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_queue_enrollment_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_queue_enrollment_tests)
bpr_enable_warnings(test_better_pull_review_review_queue_enrollment_tests)

bpr_add_network_isolated_test(
    NAME application_review_queue_enrollment
    COMMAND test_better_pull_review_review_queue_enrollment_tests
)

bpr_add_executable(
    test_better_pull_review_review_queue_notification_tests
    review_queue_notification_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_queue_notification_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_queue_notification_tests)
bpr_enable_warnings(test_better_pull_review_review_queue_notification_tests)

bpr_add_network_isolated_test(
    NAME application_review_queue_notification
    COMMAND test_better_pull_review_review_queue_notification_tests
)

bpr_add_executable(
    test_better_pull_review_agent_workspace_tests
    agent_workspace_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_workspace_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_agent_workspace_tests)
bpr_enable_warnings(test_better_pull_review_agent_workspace_tests)

bpr_add_network_isolated_test(
    NAME application_agent_workspace
    COMMAND test_better_pull_review_agent_workspace_tests
)
set_tests_properties(
    application_agent_workspace
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-workspace-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_launch_tests
    agent_launch_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_launch_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_agent_launch_tests)
bpr_enable_warnings(test_better_pull_review_agent_launch_tests)

bpr_add_network_isolated_test(
    NAME application_agent_launch
    COMMAND test_better_pull_review_agent_launch_tests
)
set_tests_properties(
    application_agent_launch
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-launch-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_protocol_loop_tests
    agent_protocol_loop_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/adapters/agent/src/agent_protocol_loop.cpp"
)
target_include_directories(
    test_better_pull_review_agent_protocol_loop_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/agent/src"
)
target_link_libraries(
    test_better_pull_review_agent_protocol_loop_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_agent_protocol_loop_tests)
bpr_enable_warnings(test_better_pull_review_agent_protocol_loop_tests)
bpr_add_network_isolated_test(
    NAME application_agent_protocol_loop
    COMMAND test_better_pull_review_agent_protocol_loop_tests
)
set_tests_properties(
    application_agent_protocol_loop
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-protocol-loop-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_invocation_host_tests
    agent_invocation_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_invocation_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_agent_invocation_host_tests)
bpr_enable_warnings(test_better_pull_review_agent_invocation_host_tests)

bpr_add_network_isolated_test(
    NAME application_agent_invocation_host
    COMMAND test_better_pull_review_agent_invocation_host_tests
)
set_tests_properties(
    application_agent_invocation_host
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-invocation-host-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_notification_delivery_tests
    notification_delivery_tests.cpp
)
target_link_libraries(
    test_better_pull_review_notification_delivery_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_notification_delivery_tests)
bpr_enable_warnings(test_better_pull_review_notification_delivery_tests)

bpr_add_network_isolated_test(
    NAME application_notification_delivery
    COMMAND test_better_pull_review_notification_delivery_tests
)
set_tests_properties(
    application_notification_delivery
    PROPERTIES
        PASS_REGULAR_EXPRESSION "notification-delivery-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_device_authorization_host_tests
    device_authorization_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_device_authorization_host_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_device_authorization_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_device_authorization_host_tests)
bpr_enable_warnings(test_better_pull_review_device_authorization_host_tests)

bpr_add_network_isolated_test(
    NAME application_device_authorization_host
    COMMAND test_better_pull_review_device_authorization_host_tests
)
set_tests_properties(
    application_device_authorization_host
    PROPERTIES
        PASS_REGULAR_EXPRESSION "device-authorization-host-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_device_authorization_refresh_tests
    device_authorization_refresh_tests.cpp
)
target_link_libraries(
    test_better_pull_review_device_authorization_refresh_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_device_authorization_refresh_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_device_authorization_refresh_tests)
bpr_enable_warnings(test_better_pull_review_device_authorization_refresh_tests)

bpr_add_network_isolated_test(
    NAME application_device_authorization_refresh
    COMMAND test_better_pull_review_device_authorization_refresh_tests
)
set_tests_properties(
    application_device_authorization_refresh
    PROPERTIES
        PASS_REGULAR_EXPRESSION "device-authorization-refresh-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_workbench_snapshot_observation_tests
    workbench_snapshot_observation_tests.cpp
)
target_link_libraries(
    test_better_pull_review_workbench_snapshot_observation_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_workbench_snapshot_observation_tests)
bpr_enable_warnings(test_better_pull_review_workbench_snapshot_observation_tests)

bpr_add_network_isolated_test(
    NAME application_workbench_snapshot_observation
    COMMAND test_better_pull_review_workbench_snapshot_observation_tests
)
set_tests_properties(
    application_workbench_snapshot_observation
    PROPERTIES
        PASS_REGULAR_EXPRESSION "workbench-snapshot-observation-tests:pass"
        TIMEOUT 60
)
