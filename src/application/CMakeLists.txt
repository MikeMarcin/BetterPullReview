file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/review-preparation/application-contract.md" WRITING_CONTRACT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/review-preparation/problem-statement.md" WRITING_PROBLEM)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/review-preparation/solution-summary.md" WRITING_SOLUTION)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/review-preparation/tour-outline.md" WRITING_OUTLINE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/review-preparation/tour-stop-comment.md" WRITING_COMMENT)
configure_file(resources/writing_hooks_data.hpp.in writing_hooks_data.hpp @ONLY)
add_library(
    better_pull_review_application
    STATIC
        src/writing_hooks.cpp
        src/review_tour_import.cpp
        src/review_tour_host.cpp
        src/review_cycle_capture.cpp
        src/review_cycle_comparison.cpp
        src/review_cycle_feedback.cpp
        src/review_cycle_round.cpp
        src/review_cycle_views.cpp
        src/review_cycle_response.cpp
        src/review_cycle_verification.cpp
        src/review_cycle_outgoing.cpp
        src/review_cycle_publication.cpp
        src/review_cycle_publisher.cpp
        src/review_cycle_export.cpp
        src/review_cycle_import.cpp
        src/review_cycle_workbench.cpp
        src/review_cycle_runtime.cpp
        src/review_tour_source.cpp
        src/window_review_restoration_host.cpp
        src/document_tab_host.cpp
        src/appearance_application_host.cpp
        src/appearance_preferences_application_host.cpp
        src/syntax_classification_application_host.cpp
		src/designated_review_authority_bootstrap.cpp
        src/review_ledger_application_host.cpp
        src/observing_review_ledger_store.cpp
        src/shared_draft_application_host.cpp
		src/review_mutation_contract.cpp
        src/review_submission_application_host.cpp
        src/review_submission_contract.cpp
        src/staged_tour_update.cpp
        src/staged_review_publication.cpp
        src/review_submission_recovery_contract.cpp
        src/review_mutation_coordinator.cpp
		src/review_mutation_application_host.cpp
        src/defect_correlation.cpp
        src/review_synchronization.cpp
        src/review_queue_refresh.cpp
        src/release_update_application_host.cpp
        src/release_update_service.cpp
        src/release_maintenance_application_host.cpp
        src/release_diagnostic_snapshot.cpp
        src/diagnostic_settings.cpp
        src/release_profile_cleanup.cpp
        src/personal_discovery_host.cpp
        src/review_queue_enrollment.cpp
        src/setup_destination.cpp
        src/profile_registry.cpp
        src/profile_storage_authority.cpp
        src/profile_startup_admission.cpp
        src/profile_management_host.cpp
        src/agent_chat_session_host.cpp
        src/agent_comment_workshop_host.cpp
        src/agent_finding_delivery.cpp
        src/agent_finding_host.cpp
        src/agent_launch.cpp
		src/agent_invocation_host.cpp
        src/private_review_application_host.cpp
    src/review_composition_application_host.cpp
    src/private_publication_application_host.cpp
        src/agent_realignment.cpp
        src/agent_workspace.cpp
        src/mcp_context_helper_supervisor.cpp
        src/notification_delivery.cpp
        src/review_queue_notification.cpp
        src/workbench_snapshot_observation.cpp
        src/diff_application_host.cpp
        src/device_authorization_application_host.cpp
        src/device_binding_store.cpp
        src/device_authorization_schedule.cpp
        src/github_capability_cache.cpp
        src/review_publication_capability_cache.cpp
        src/review_publication_recovery_contract.cpp
        src/epoch_application_host.cpp
        src/profile_epoch_actor_registry.cpp
        src/epoch_runtime_ports.cpp
        src/github_application_host.cpp
        src/github_window_read_host.cpp
        src/github_read_admission.cpp
        src/github_credential_vault.cpp
        src/github_projection_mapping.cpp
        src/review_comment_placement.cpp
        src/pull_request_overview.cpp
        src/git_protocol_v2.cpp
        src/git_smart_transport_port.cpp
        src/host.cpp
        src/manual_executor.cpp
        src/workbench_application_host.cpp
        src/workbench_preferences_host.cpp
)
add_library(BetterPullReview::Application ALIAS better_pull_review_application)

target_link_libraries(
    better_pull_review_application
    PUBLIC
        BetterPullReview::Core
)
target_include_directories(
    better_pull_review_application
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_application PUBLIC cxx_std_23)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_application
        PUBLIC
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
if(WIN32)
    target_link_libraries(
        better_pull_review_application
        PRIVATE
            Bcrypt
    )
endif()

if(BUILD_TESTING)
    target_compile_definitions(
        better_pull_review_application
        PRIVATE
            BPR_APPLICATION_TESTING=1
    )
endif()

bpr_enable_hardening(better_pull_review_application)
bpr_enable_warnings(better_pull_review_application)

target_include_directories(better_pull_review_application PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
