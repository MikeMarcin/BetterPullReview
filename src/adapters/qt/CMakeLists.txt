add_library(
    better_pull_review_adapter_qt
    STATIC
        src/agent_launch_panel.cpp
        src/appearance_profile.cpp
        src/appearance_settings_panel.cpp
        src/github_session_panel.cpp
        src/system_tray_notification_sink.cpp
        src/review_queue_model.cpp
        src/review_landing_model.cpp
        src/defect_card_view.cpp
        src/device_authorization_panel.cpp
        src/review_draft_editor.cpp
        src/review_submission_editor.cpp
        src/settings_window.cpp
        src/workbench_strip.cpp
        src/native_window_chrome.cpp
        src/comparison_scroll_controller.cpp
        src/diff_link_map.cpp
		src/diff_workbench.cpp
        src/finding_queue_view.cpp
        src/event_loop_executor.cpp
        src/event_loop_device_timer.cpp
        src/epoch_comparison_control.cpp
    src/menu_tool_button.cpp
        src/epoch_history_model.cpp
        src/foundation_window.cpp
        src/github_conversation_delegate.cpp
        src/github_conversation_model.cpp
        src/github_thread_map.cpp
    src/github_conversation_panel.cpp
    src/github_pull_request_panel.cpp
        src/measured_alignment_axis.cpp
        src/sensitive_text_input.cpp
        src/side_chat_panel.cpp
        src/usage_ring.cpp
        src/workbench_drawer.cpp
        src/workbench_window.cpp
)
add_library(BetterPullReview::AdapterQt ALIAS better_pull_review_adapter_qt)

target_link_libraries(
    better_pull_review_adapter_qt
    PUBLIC
        BetterPullReview::Application
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
target_include_directories(
    better_pull_review_adapter_qt
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_adapter_qt PUBLIC cxx_std_23)

bpr_enable_hardening(better_pull_review_adapter_qt)
bpr_enable_warnings(better_pull_review_adapter_qt)

if(APPLE)
    # native_window_chrome.cpp reaches the NSWindow through the Objective-C
    # runtime from C++; the runtime is not linked by the Qt targets.
    target_link_libraries(better_pull_review_adapter_qt PRIVATE objc)
endif()
