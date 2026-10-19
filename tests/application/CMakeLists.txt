bpr_add_executable(test_release_update_host release_update_application_host_tests.cpp)
target_link_libraries(test_release_update_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_release_update_host)
bpr_enable_warnings(test_release_update_host)
bpr_add_network_isolated_test(NAME application_release_update COMMAND test_release_update_host)

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
    test_better_pull_review_application_anonymous_read_host_tests
    github_anonymous_read_host_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_anonymous_read_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_anonymous_read_host_tests)
bpr_enable_warnings(test_better_pull_review_application_anonymous_read_host_tests)

bpr_add_network_isolated_test(
    NAME application_anonymous_read_host
    COMMAND test_better_pull_review_application_anonymous_read_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_anonymous_diff_authority_tests
    anonymous_diff_authority_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_anonymous_diff_authority_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_anonymous_diff_authority_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_anonymous_diff_authority_tests
)

bpr_add_network_isolated_test(
    NAME application_anonymous_diff_authority
    COMMAND test_better_pull_review_application_anonymous_diff_authority_tests
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

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
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
endif()

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

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(
    test_better_pull_review_agent_launch_tests
    agent_launch_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_launch_tests
    PRIVATE
        BetterPullReview::Application
)
# "The ungated overload is no longer reachable from the host" is a claim about
# what the host does not call, and no value it returns can show it. The suite
# reads the one translation unit that could call it.
target_compile_definitions(
    test_better_pull_review_agent_launch_tests
    PRIVATE
        "BPR_AGENT_LAUNCH_SOURCE=\"${PROJECT_SOURCE_DIR}/src/application/src/agent_launch.cpp\""
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

# [[BPR-1180]]. The Send to Agent launch per protocol, and how a run's end is read.
bpr_add_executable(test_better_pull_review_review_cycle_round_launch_tests review_cycle_round_launch_tests.cpp)
target_link_libraries(test_better_pull_review_review_cycle_round_launch_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_review_cycle_round_launch_tests)
bpr_enable_warnings(test_better_pull_review_review_cycle_round_launch_tests)
bpr_add_network_isolated_test(NAME application_review_cycle_round_launch COMMAND test_better_pull_review_review_cycle_round_launch_tests)
set_tests_properties(application_review_cycle_round_launch PROPERTIES PASS_REGULAR_EXPRESSION "review-cycle-round-launch-tests:pass")

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
    test_better_pull_review_agent_finding_protocol_tests
    agent_finding_protocol_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/adapters/agent/src/agent_protocol_loop.cpp"
)
target_include_directories(
    test_better_pull_review_agent_finding_protocol_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/agent/src"
)
target_link_libraries(
    test_better_pull_review_agent_finding_protocol_tests
    PRIVATE
        BetterPullReview::Application
)
# The real child fixture only exists where Qt is configured, because that is
# where the process adapter it serves exists. Without it the suite still proves
# the wire contract and every typed cause; it reports the absence rather than
# passing quietly on a course it did not run.
if(BPR_BUILD_QT)
    target_compile_definitions(
        test_better_pull_review_agent_finding_protocol_tests
        PRIVATE
            "BPR_AGENT_CANARY_EXECUTABLE=\"$<TARGET_FILE:test_better_pull_review_agent_canary>\""
    )
    add_dependencies(
        test_better_pull_review_agent_finding_protocol_tests
        test_better_pull_review_agent_canary
    )
endif()
bpr_enable_hardening(test_better_pull_review_agent_finding_protocol_tests)
bpr_enable_warnings(test_better_pull_review_agent_finding_protocol_tests)

bpr_add_network_isolated_test(
    NAME application_agent_finding_protocol
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_agent_finding_protocol_tests
)
set_tests_properties(
    application_agent_finding_protocol
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-finding-protocol-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_agent_comment_workshop_host_tests
    agent_comment_workshop_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_agent_comment_workshop_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_agent_comment_workshop_host_tests)
bpr_enable_warnings(test_better_pull_review_agent_comment_workshop_host_tests)

bpr_add_network_isolated_test(
    NAME application_agent_comment_workshop_host
    COMMAND test_better_pull_review_agent_comment_workshop_host_tests
)
set_tests_properties(
    application_agent_comment_workshop_host
    PROPERTIES
        PASS_REGULAR_EXPRESSION "agent-comment-workshop-host-tests:pass"
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
endif()

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

bpr_add_executable(
    test_better_pull_review_pull_request_overview_tests
    pull_request_overview_tests.cpp
)
target_link_libraries(
    test_better_pull_review_pull_request_overview_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_pull_request_overview_tests)
bpr_enable_warnings(test_better_pull_review_pull_request_overview_tests)

bpr_add_network_isolated_test(
    NAME application_pull_request_overview
    COMMAND test_better_pull_review_pull_request_overview_tests
)
set_tests_properties(
    application_pull_request_overview
    PROPERTIES
        PASS_REGULAR_EXPRESSION "pull-request-overview-tests:pass"
        TIMEOUT 60
)

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
# The MCP context helper's evidence is what a separate process reaches, so the
# canary drives the shipped executable over real stdio rather than calling the
# library in-process. It links the adapter as well, because the advertised tool
# list is compared against the compiled enumeration and the source text.
bpr_add_executable(
    test_better_pull_review_mcp_context_helper_tests
    mcp_context_helper_tests.cpp
)
target_link_libraries(
    test_better_pull_review_mcp_context_helper_tests
    PRIVATE
        BetterPullReview::AdapterMcp
)
target_compile_definitions(
    test_better_pull_review_mcp_context_helper_tests
    PRIVATE
        "BPR_MCP_HELPER_EXECUTABLE=\"$<TARGET_FILE:tool_better_pull_review_mcp_context_helper>\""
        "BPR_MCP_TOOL_SOURCE=\"${PROJECT_SOURCE_DIR}/src/adapters/mcp/include/bpr/adapters/mcp/mcp_context_helper.hpp\""
)
add_dependencies(
    test_better_pull_review_mcp_context_helper_tests
    tool_better_pull_review_mcp_context_helper
)
bpr_enable_hardening(test_better_pull_review_mcp_context_helper_tests)
bpr_enable_warnings(test_better_pull_review_mcp_context_helper_tests)

# No LOOPBACK_PORT_COUNT: the helper declares no socket surface at all, so the
# default of zero is the claim rather than an omission.
# BPR-0522: the suite's three filesystem rows reach weakly_canonical on the
# snapshot root, and a lowbox token refuses the drive-letter translation that
# needs, so inside the AppContainer the helper answers snapshot_unavailable
# where path_outside_snapshot is expected (BPR-0362's cause, measured on the
# Windows station); outside it the whole suite passes. The helper itself
# opens no socket, so the exemption costs no network claim.
bpr_add_network_isolated_test(
    NAME application_mcp_context_helper
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_mcp_context_helper_tests
)
set_tests_properties(
    application_mcp_context_helper
    PROPERTIES
        PASS_REGULAR_EXPRESSION "mcp-context-helper-tests:pass"
        TIMEOUT 120
)

# Exercise the shipped helper with hostile environment names and full/closed
# stderr pipes. The Windows synchronous-write course is explicitly pending.
if(UNIX)
    bpr_add_network_isolated_test(
        NAME application_mcp_terminal_diagnostics
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/audit_terminal_diagnostics.py"
            --helper "$<TARGET_FILE:tool_better_pull_review_mcp_context_helper>"
    )
    set_tests_properties(application_mcp_terminal_diagnostics PROPERTIES TIMEOUT 30)
endif()
endif()

bpr_add_executable(
    test_better_pull_review_review_submission_contract_application_tests
    review_submission_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_submission_contract_application_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_review_submission_contract_application_tests)
bpr_enable_warnings(test_better_pull_review_review_submission_contract_application_tests)
bpr_add_network_isolated_test(
    NAME application_review_submission_contract
    COMMAND test_better_pull_review_review_submission_contract_application_tests
)


bpr_add_executable(test_better_pull_review_review_submission_application_host_tests
    review_submission_application_host_tests.cpp)
target_link_libraries(test_better_pull_review_review_submission_application_host_tests
    PRIVATE BetterPullReview::Application)
target_include_directories(test_better_pull_review_review_submission_application_host_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/application/src")
target_compile_definitions(test_better_pull_review_review_submission_application_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1)
bpr_enable_hardening(test_better_pull_review_review_submission_application_host_tests)
bpr_enable_warnings(test_better_pull_review_review_submission_application_host_tests)
bpr_add_network_isolated_test(NAME application_review_submission_application_host
    COMMAND test_better_pull_review_review_submission_application_host_tests)

bpr_add_executable(
    test_better_pull_review_application_setup_destination_tests
    setup_destination_tests.cpp
)
target_link_libraries(test_better_pull_review_application_setup_destination_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_setup_destination_tests)
bpr_enable_warnings(test_better_pull_review_application_setup_destination_tests)
bpr_add_network_isolated_test(
    NAME application_setup_destination
    COMMAND test_better_pull_review_application_setup_destination_tests
)

bpr_add_executable(test_better_pull_review_application_profile_registry_tests profile_registry_tests.cpp)
target_link_libraries(test_better_pull_review_application_profile_registry_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_profile_registry_tests)
bpr_enable_warnings(test_better_pull_review_application_profile_registry_tests)
bpr_add_network_isolated_test(NAME application_profile_registry COMMAND test_better_pull_review_application_profile_registry_tests)
set_tests_properties(application_profile_registry PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_application_profile_storage_authority_tests profile_storage_authority_tests.cpp)
target_link_libraries(test_better_pull_review_application_profile_storage_authority_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_profile_storage_authority_tests)
bpr_enable_warnings(test_better_pull_review_application_profile_storage_authority_tests)
bpr_add_network_isolated_test(NAME application_profile_storage_authority COMMAND test_better_pull_review_application_profile_storage_authority_tests)
set_tests_properties(application_profile_storage_authority PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_application_profile_startup_admission_tests profile_startup_admission_tests.cpp)
target_link_libraries(test_better_pull_review_application_profile_startup_admission_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_profile_startup_admission_tests)
bpr_enable_warnings(test_better_pull_review_application_profile_startup_admission_tests)
bpr_add_network_isolated_test(NAME application_profile_startup_admission COMMAND test_better_pull_review_application_profile_startup_admission_tests)
set_tests_properties(application_profile_startup_admission PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_application_profile_management_host_tests profile_management_host_tests.cpp)
target_link_libraries(test_better_pull_review_application_profile_management_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_profile_management_host_tests)
bpr_enable_warnings(test_better_pull_review_application_profile_management_host_tests)
bpr_add_network_isolated_test(NAME application_profile_management_host COMMAND test_better_pull_review_application_profile_management_host_tests)
set_tests_properties(application_profile_management_host PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_application_github_window_read_host_tests github_window_read_host_tests.cpp)
target_link_libraries(test_better_pull_review_application_github_window_read_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_github_window_read_host_tests)
bpr_enable_warnings(test_better_pull_review_application_github_window_read_host_tests)
bpr_add_network_isolated_test(NAME application_github_window_read_host COMMAND test_better_pull_review_application_github_window_read_host_tests)

bpr_add_executable(test_better_pull_review_application_appearance_profile_consumers_tests appearance_profile_consumers_tests.cpp)
target_link_libraries(test_better_pull_review_application_appearance_profile_consumers_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_appearance_profile_consumers_tests)
bpr_enable_warnings(test_better_pull_review_application_appearance_profile_consumers_tests)
bpr_add_network_isolated_test(NAME application_appearance_profile_consumers COMMAND test_better_pull_review_application_appearance_profile_consumers_tests)

bpr_add_executable(test_better_pull_review_application_workbench_preferences_consumers_tests workbench_preferences_consumers_tests.cpp)
target_link_libraries(test_better_pull_review_application_workbench_preferences_consumers_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_workbench_preferences_consumers_tests)
bpr_enable_warnings(test_better_pull_review_application_workbench_preferences_consumers_tests)
bpr_add_network_isolated_test(NAME application_workbench_preferences_consumers COMMAND test_better_pull_review_application_workbench_preferences_consumers_tests)

bpr_add_executable(test_better_pull_review_shared_draft_application_host_tests shared_draft_application_host_tests.cpp)
target_link_libraries(test_better_pull_review_shared_draft_application_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_shared_draft_application_host_tests)
bpr_enable_warnings(test_better_pull_review_shared_draft_application_host_tests)
bpr_add_network_isolated_test(NAME application_shared_draft_application_host COMMAND test_better_pull_review_shared_draft_application_host_tests)

bpr_add_executable(test_better_pull_review_window_review_restoration_host_tests window_review_restoration_host_tests.cpp)
target_link_libraries(test_better_pull_review_window_review_restoration_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_window_review_restoration_host_tests)
bpr_enable_warnings(test_better_pull_review_window_review_restoration_host_tests)
bpr_add_network_isolated_test(NAME application_window_review_restoration COMMAND test_better_pull_review_window_review_restoration_host_tests)
set_tests_properties(application_window_review_restoration PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_document_tab_host_tests document_tab_host_tests.cpp)
target_link_libraries(test_better_pull_review_document_tab_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_document_tab_host_tests)
bpr_enable_warnings(test_better_pull_review_document_tab_host_tests)
bpr_add_network_isolated_test(NAME application_document_tab_host COMMAND test_better_pull_review_document_tab_host_tests)
set_tests_properties(application_document_tab_host PROPERTIES TIMEOUT 60)

bpr_add_executable(test_source_search_host source_search_host_tests.cpp)
target_link_libraries(test_source_search_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_source_search_host)
bpr_enable_warnings(test_source_search_host)
bpr_add_network_isolated_test(NAME application_source_search_host COMMAND test_source_search_host)

bpr_add_executable(test_release_maintenance_host release_maintenance_host_tests.cpp)
target_link_libraries(test_release_maintenance_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_release_maintenance_host)
bpr_enable_warnings(test_release_maintenance_host)
bpr_add_network_isolated_test(NAME application_release_maintenance COMMAND test_release_maintenance_host)
set_tests_properties(application_release_maintenance PROPERTIES TIMEOUT 30)

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(test_better_pull_review_agent_chat_reply_tests agent_chat_reply_tests.cpp)
target_link_libraries(test_better_pull_review_agent_chat_reply_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_agent_chat_reply_tests)
bpr_enable_warnings(test_better_pull_review_agent_chat_reply_tests)
bpr_add_network_isolated_test(NAME application_agent_chat_replies COMMAND test_better_pull_review_agent_chat_reply_tests)
set_tests_properties(application_agent_chat_replies PROPERTIES PASS_REGULAR_EXPRESSION "agent-chat-reply-tests:pass" TIMEOUT 30)
endif()

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1301]]
bpr_add_executable(test_better_pull_review_claude_process_wrapper_host_tests claude_process_wrapper_tests.cpp)
target_link_libraries(test_better_pull_review_claude_process_wrapper_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_claude_process_wrapper_host_tests)
bpr_enable_warnings(test_better_pull_review_claude_process_wrapper_host_tests)
bpr_add_network_isolated_test(NAME application_claude_process_wrapper COMMAND test_better_pull_review_claude_process_wrapper_host_tests)
set_tests_properties(application_claude_process_wrapper PROPERTIES PASS_REGULAR_EXPRESSION "claude-process-wrapper-host-tests:pass" TIMEOUT 30)
endif()

bpr_add_executable(test_review_tour_host review_tour_host_tests.cpp)
target_link_libraries(test_review_tour_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_review_tour_host)
bpr_enable_warnings(test_review_tour_host)
bpr_add_network_isolated_test(NAME application_review_tour_host COMMAND test_review_tour_host)

bpr_add_executable(test_review_publication_capability_cache review_publication_capability_cache_tests.cpp)
target_link_libraries(test_review_publication_capability_cache PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_review_publication_capability_cache)
bpr_enable_warnings(test_review_publication_capability_cache)
bpr_add_network_isolated_test(NAME application_review_publication_capability_cache COMMAND test_review_publication_capability_cache)

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(test_writing_delivery writing_delivery_tests.cpp)
target_link_libraries(test_writing_delivery PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_writing_delivery)
bpr_enable_warnings(test_writing_delivery)
bpr_add_network_isolated_test(NAME application_writing_delivery COMMAND test_writing_delivery)

endif()
# [[BPR-1087]]. Accepted label buckets per pull request, file and folder from the
# accepted read model, with the kept annotations and the bounded re-tally.
bpr_add_executable(test_better_pull_review_feedback_counters_tests feedback_counters_tests.cpp)
target_link_libraries(test_better_pull_review_feedback_counters_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_feedback_counters_tests)
bpr_enable_warnings(test_better_pull_review_feedback_counters_tests)
bpr_add_network_isolated_test(NAME application_feedback_counters COMMAND test_better_pull_review_feedback_counters_tests)
set_tests_properties(application_feedback_counters PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "feedback-counters-tests:pass")
# [[BPR-1089]]. The Overview's label chips from the kept buckets: every row's
# numbers, the exact drill-down, the numeric sort and qualified unknown counts.
bpr_add_executable(test_better_pull_review_overview_feedback_tests overview_feedback_tests.cpp)
target_link_libraries(test_better_pull_review_overview_feedback_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_overview_feedback_tests)
bpr_enable_warnings(test_better_pull_review_overview_feedback_tests)
bpr_add_network_isolated_test(NAME application_overview_feedback COMMAND test_better_pull_review_overview_feedback_tests)
set_tests_properties(application_overview_feedback PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "overview-feedback-tests:pass")

# [[BPR-1094]]. What the label counters cost on 1,000 and 10,000 synthetic
# conversations, with no chips, three and all twelve: work, allocations and
# time are reported; only structural facts are asserted. Serial, so its times
# are not shared with another row.
bpr_add_executable(test_better_pull_review_feedback_counter_costs feedback_counter_cost_tests.cpp)
target_link_libraries(test_better_pull_review_feedback_counter_costs PRIVATE BetterPullReview::Application)
target_compile_definitions(test_better_pull_review_feedback_counter_costs PRIVATE "BPR_BENCH_BUILD_CONFIGURATION=\"$<CONFIG>\"")
bpr_enable_hardening(test_better_pull_review_feedback_counter_costs)
bpr_enable_warnings(test_better_pull_review_feedback_counter_costs)
bpr_add_network_isolated_test(NAME application_feedback_counter_costs COMMAND test_better_pull_review_feedback_counter_costs)
set_tests_properties(application_feedback_counter_costs PROPERTIES RUN_SERIAL TRUE TIMEOUT 300 PASS_REGULAR_EXPRESSION "feedback-counter-costs:measured")
# [[BPR-1447]]. The body digest port: every backend equals the core's SHA-256,
# and supplied digests count exactly as core-hashed ones with no body hashed on
# the window thread. The platform backend lives in the GitHub adapter, which a
# core-only tree does not build; there only the core backend is checked.
bpr_add_executable(test_better_pull_review_body_digest_port body_digest_port_tests.cpp)
target_link_libraries(test_better_pull_review_body_digest_port PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_body_digest_port)
bpr_enable_warnings(test_better_pull_review_body_digest_port)
bpr_add_network_isolated_test(NAME application_body_digest_port COMMAND test_better_pull_review_body_digest_port)
set_tests_properties(application_body_digest_port PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "body-digest-port-tests:pass")
if(BPR_BUILD_QT OR BPR_BUILD_TOUR_CLI)
    foreach(bpr_body_digest_target IN ITEMS test_better_pull_review_body_digest_port test_better_pull_review_feedback_counter_costs)
        target_link_libraries(${bpr_body_digest_target} PRIVATE BetterPullReview::AdapterGitHub)
        target_compile_definitions(${bpr_body_digest_target} PRIVATE BPR_HAS_PLATFORM_BODY_DIGEST=1)
        target_include_directories(${bpr_body_digest_target} PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    endforeach()
endif()

bpr_add_executable(test_better_pull_review_application_home_open_host_tests home_open_host_tests.cpp)
target_link_libraries(test_better_pull_review_application_home_open_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_home_open_host_tests)
bpr_enable_warnings(test_better_pull_review_application_home_open_host_tests)
bpr_add_network_isolated_test(NAME application_home_open_host COMMAND test_better_pull_review_application_home_open_host_tests)

bpr_add_executable(test_remembered_connection_host remembered_connection_host_tests.cpp)
target_link_libraries(test_remembered_connection_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_remembered_connection_host)
bpr_enable_warnings(test_remembered_connection_host)
bpr_add_network_isolated_test(NAME application_remembered_connection_host COMMAND test_remembered_connection_host)

bpr_add_executable(test_source_pr_search source_pr_search_tests.cpp)
target_link_libraries(test_source_pr_search PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_source_pr_search)
bpr_enable_warnings(test_source_pr_search)
bpr_add_network_isolated_test(NAME application_source_pr_search COMMAND test_source_pr_search)

bpr_add_executable(test_better_pull_review_application_markdown_image_host_tests markdown_image_host_tests.cpp)
target_link_libraries(test_better_pull_review_application_markdown_image_host_tests PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_better_pull_review_application_markdown_image_host_tests)
bpr_enable_warnings(test_better_pull_review_application_markdown_image_host_tests)
bpr_add_network_isolated_test(NAME application_markdown_image_host COMMAND test_better_pull_review_application_markdown_image_host_tests)

# [[BPR-1562]]. The Markdown reader's code classification host.
bpr_add_executable(
    test_better_pull_review_application_markdown_code_syntax_host_tests
    markdown_code_syntax_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_markdown_code_syntax_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_markdown_code_syntax_host_tests)
bpr_enable_warnings(test_better_pull_review_application_markdown_code_syntax_host_tests)

bpr_add_network_isolated_test(
    NAME application_markdown_code_syntax_host
    COMMAND test_better_pull_review_application_markdown_code_syntax_host_tests
)

bpr_add_executable(test_insights_host insights_host_tests.cpp)
target_link_libraries(test_insights_host PRIVATE BetterPullReview::Application)
bpr_enable_hardening(test_insights_host)
bpr_enable_warnings(test_insights_host)
bpr_add_network_isolated_test(NAME application_insights_host COMMAND test_insights_host)
