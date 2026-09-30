add_library(
    better_pull_review_adapter_qt
    STATIC
        src/review_tour_editor.cpp
        src/scoped_prose_editor.cpp
        src/suggestion_editor.cpp
        src/suggestion_composer.cpp
        src/writing_hooks_panel.cpp
        src/release_maintenance_panel.cpp
        src/release_update_panel.cpp
        src/release_update_recovery_dialog.cpp
        src/agent_launch_panel.cpp
        src/appearance_profile.cpp
        src/appearance_settings_panel.cpp
        src/github_cli_consent.cpp
        src/github_session_panel.cpp
        src/system_tray_notification_sink.cpp
        src/review_queue_model.cpp
        src/review_landing_model.cpp
        src/home_row_delegate.cpp
        src/home_panel.cpp
        src/home_list.cpp
        src/home_refusal.cpp
        src/defect_card_view.cpp
        src/private_notes_panel.cpp
        src/desktop_menu_bar.cpp
        src/device_authorization_panel.cpp
        src/dif_runtime.cpp
        src/motion_observer.cpp
        src/review_draft_editor.cpp
        src/review_submission_editor.cpp
        src/settings_window.cpp
            src/diagnostic_settings_panel.cpp
        src/profile_management_window.cpp
        src/workbench_strip.cpp
        src/document_tab_strip.cpp
        src/native_window_chrome.cpp
        src/epoch_line.cpp
        src/workbench_landing.cpp
        src/markdown_body_view.cpp
        src/pull_request_overview_page.cpp
        src/conversation_cards_view.cpp
        src/repositories_drawer.cpp
        src/files_tree.cpp
        src/resident_filter.cpp
        src/theme_preview_strip.cpp
        src/code_sample_painter.cpp
        src/type_preview.cpp
        src/appearance_role_display.cpp
        src/colour_role_list.cpp
        src/info_glyph.cpp
        src/server_card.cpp
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
        src/private_review_panel.cpp
        src/private_composition_editor.cpp
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

# [[BPR-0653]]. The Windows title row's caption buttons: a Windows-only
# presenter; the host that answers the platform's hit test is in src/app.
if(WIN32)
    target_sources(better_pull_review_adapter_qt PRIVATE src/windows_caption_buttons.cpp)
endif()
if(APPLE)
    enable_language(OBJCXX)
    target_sources(better_pull_review_adapter_qt PRIVATE src/motion_observer_mac.mm src/release_update_menu_mac.mm)
    set_source_files_properties(src/motion_observer_mac.mm src/release_update_menu_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    find_library(BPR_DIF_APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(better_pull_review_adapter_qt PRIVATE "${BPR_DIF_APPKIT_FRAMEWORK}")
    # native_window_chrome.cpp reaches the NSWindow through the Objective-C
    # runtime from C++; the runtime is not linked by the Qt targets.
    target_link_libraries(better_pull_review_adapter_qt PRIVATE objc)
endif()

# Accepted committed production products only; no asset generation in ordinary builds.
# Stored uncompressed, which the runtime requires. [[BPR-0974]]: a tests-disabled
# Windows image carries them in its resource bundle instead (cmake/BprQtResources.cmake).
if(BPR_EXTERNAL_QT_RESOURCES)
    target_compile_definitions(better_pull_review_adapter_qt PRIVATE BPR_EXTERNAL_QT_RESOURCES=1)
endif()
bpr_add_qt_resources(better_pull_review_adapter_qt bpr_dif
    PREFIX "/bpr/dif"
    BASE "assets/dif/generated"
    NO_COMPRESS
    FILES
        assets/dif/generated/dif-rest.png
        assets/dif/generated/dif-calm-idle.png
        assets/dif/generated/dif-greet.png
        assets/dif/generated/dif-blink.png
        assets/dif/generated/dif-compare-glance.png
        assets/dif/generated/dif-consider.png
        assets/dif/generated/dif-comment-ready.png
        assets/dif/generated/dif-attention.png
        assets/dif/generated/dif-acknowledge.png
        assets/dif/generated/dif-blocked.png
)
