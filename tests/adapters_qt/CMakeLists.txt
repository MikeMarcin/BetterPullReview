bpr_add_executable(
    test_better_pull_review_qt_tests
    foundation_window_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_tests)
bpr_enable_warnings(test_better_pull_review_qt_tests)

bpr_add_network_isolated_test(NAME qt_foundation_window COMMAND test_better_pull_review_qt_tests)
set_tests_properties(
    qt_foundation_window
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
)

bpr_add_executable(
    test_better_pull_review_qt_system_tray_notification_sink_tests
    system_tray_notification_sink_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_system_tray_notification_sink_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(
    test_better_pull_review_qt_system_tray_notification_sink_tests
)
bpr_enable_warnings(
    test_better_pull_review_qt_system_tray_notification_sink_tests
)
bpr_add_network_isolated_test(
    NAME qt_system_tray_notification_sink
    COMMAND test_better_pull_review_qt_system_tray_notification_sink_tests
)
set_tests_properties(
    qt_system_tray_notification_sink
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "system-tray-notification-sink-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_qt_github_tests
    composed_github_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_github_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_github_tests)
bpr_enable_warnings(test_better_pull_review_qt_github_tests)

set(bpr_qt_github_common_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    # Qt's basic style avoids a Fusion cached-painter fault in fractional-scale
    # offscreen rendering. Native-style UX validation remains a separate gate.
    list(APPEND bpr_qt_github_common_environment "QT_STYLE_OVERRIDE=Windows")
endif()

bpr_add_network_isolated_test(
    NAME qt_composed_github_scale_100
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_composed_github_scale_100
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.0;BPR_EXPECTED_QT_SCALE=1.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-100.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-lower-100.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-100.png"
        TIMEOUT 120
)

bpr_add_network_isolated_test(
    NAME qt_composed_github_scale_125
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_composed_github_scale_125
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.25;BPR_EXPECTED_QT_SCALE=1.25;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-125.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-lower-125.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-125.png"
        TIMEOUT 120
)

bpr_add_network_isolated_test(
    NAME qt_composed_github_scale_200
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_composed_github_scale_200
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=2.0;BPR_EXPECTED_QT_SCALE=2.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-200.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-minimum-lower-200.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/composed-github-200.png"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_device_authorization_panel_tests
    device_authorization_panel_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_device_authorization_panel_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_device_authorization_panel_tests)
bpr_enable_warnings(test_better_pull_review_qt_device_authorization_panel_tests)

foreach(bpr_device_authorization_scale IN ITEMS 100 125 200)
    if(bpr_device_authorization_scale STREQUAL "100")
        set(bpr_device_authorization_scale_factor "1.0")
    elseif(bpr_device_authorization_scale STREQUAL "125")
        set(bpr_device_authorization_scale_factor "1.25")
    else()
        set(bpr_device_authorization_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_device_authorization_scale_${bpr_device_authorization_scale}
        COMMAND test_better_pull_review_qt_device_authorization_panel_tests
    )
    set_tests_properties(
        qt_device_authorization_scale_${bpr_device_authorization_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_device_authorization_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_device_authorization_scale_factor}"
            PASS_REGULAR_EXPRESSION "device-authorization-panel-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_anonymous_public_preview_tests
    anonymous_public_preview_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_anonymous_public_preview_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_anonymous_public_preview_tests)
bpr_enable_warnings(test_better_pull_review_qt_anonymous_public_preview_tests)

foreach(bpr_public_preview_scale IN ITEMS 100 125 200)
    if(bpr_public_preview_scale STREQUAL "100")
        set(bpr_public_preview_scale_factor "1.0")
    elseif(bpr_public_preview_scale STREQUAL "125")
        set(bpr_public_preview_scale_factor "1.25")
    else()
        set(bpr_public_preview_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_anonymous_public_preview_scale_${bpr_public_preview_scale}
        COMMAND test_better_pull_review_qt_anonymous_public_preview_tests
    )
    set_tests_properties(
        qt_anonymous_public_preview_scale_${bpr_public_preview_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_public_preview_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_public_preview_scale_factor}"
            PASS_REGULAR_EXPRESSION "anonymous-public-preview-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_github_conversation_panel_tests
    github_conversation_panel_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_github_conversation_panel_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(
    test_better_pull_review_qt_github_conversation_panel_tests
)
bpr_enable_warnings(
    test_better_pull_review_qt_github_conversation_panel_tests
)

bpr_add_network_isolated_test(
    NAME qt_github_conversation_panel
    COMMAND test_better_pull_review_qt_github_conversation_panel_tests
)
set_tests_properties(
    qt_github_conversation_panel
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "conversation-panel-tests:pass"
)

bpr_add_executable(
    test_better_pull_review_qt_conversation_cards_view_tests
    conversation_cards_view_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_conversation_cards_view_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterQt
)
target_compile_definitions(
    test_better_pull_review_qt_conversation_cards_view_tests
    PRIVATE
        "BPR_BRANCHING_THREAD_FIXTURE=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/conversations/branching-thread-v1/projection.json\""
)
bpr_enable_hardening(test_better_pull_review_qt_conversation_cards_view_tests)
bpr_enable_warnings(test_better_pull_review_qt_conversation_cards_view_tests)

bpr_add_network_isolated_test(
    NAME qt_conversation_cards_view
    COMMAND test_better_pull_review_qt_conversation_cards_view_tests
)
set_tests_properties(
    qt_conversation_cards_view
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "conversation-cards-view-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_qt_files_tree_tests
    files_tree_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_files_tree_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_files_tree_tests)
bpr_enable_warnings(test_better_pull_review_qt_files_tree_tests)

bpr_add_network_isolated_test(
    NAME qt_files_tree
    COMMAND test_better_pull_review_qt_files_tree_tests
)
set_tests_properties(
    qt_files_tree
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "files-tree-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_qt_repositories_drawer_tests
    repositories_drawer_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_repositories_drawer_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_repositories_drawer_tests)
bpr_enable_warnings(test_better_pull_review_qt_repositories_drawer_tests)

bpr_add_network_isolated_test(
    NAME qt_repositories_drawer
    COMMAND test_better_pull_review_qt_repositories_drawer_tests
)
set_tests_properties(
    qt_repositories_drawer
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "repositories-drawer-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_qt_github_thread_map_tests
    github_thread_map_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_github_thread_map_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterQt
)
target_compile_definitions(
    test_better_pull_review_qt_github_thread_map_tests
    PRIVATE
        "BPR_DEEP_THREAD_FIXTURE=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/conversations/deep-thread-v1/projection.json\""
)
bpr_enable_hardening(
    test_better_pull_review_qt_github_thread_map_tests
)
bpr_enable_warnings(
    test_better_pull_review_qt_github_thread_map_tests
)

# Three scales, because the map sizes every node to its own label and a
# fractional device pixel ratio is where a layout that assumed integer metrics
# stops agreeing with what it painted.
foreach(bpr_thread_map_scale 100 125 200)
    if(bpr_thread_map_scale STREQUAL "100")
        set(bpr_thread_map_factor 1.0)
    elseif(bpr_thread_map_scale STREQUAL "125")
        set(bpr_thread_map_factor 1.25)
    else()
        set(bpr_thread_map_factor 2.0)
    endif()
    bpr_add_network_isolated_test(
        NAME "qt_github_thread_map_scale_${bpr_thread_map_scale}"
        COMMAND test_better_pull_review_qt_github_thread_map_tests
    )
    set_tests_properties(
        "qt_github_thread_map_scale_${bpr_thread_map_scale}"
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_thread_map_factor};BPR_EXPECTED_QT_SCALE=${bpr_thread_map_factor};BPR_QT_THREAD_MAP_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-thread-map-${bpr_thread_map_scale}.png"
            PASS_REGULAR_EXPRESSION "thread-map-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_github_conversation_model_property_tests
    github_conversation_model_property_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_github_conversation_model_property_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(
    test_better_pull_review_qt_github_conversation_model_property_tests
)
bpr_enable_warnings(
    test_better_pull_review_qt_github_conversation_model_property_tests
)

bpr_add_network_isolated_test(
    NAME qt_github_conversation_model_property
    COMMAND test_better_pull_review_qt_github_conversation_model_property_tests
)
set_tests_properties(
    qt_github_conversation_model_property
    PROPERTIES
        PASS_REGULAR_EXPRESSION
            "github-conversation-model-property-tests:pass"
        TIMEOUT 180
)

bpr_add_executable(
    test_better_pull_review_qt_sensitive_text_input_tests
    sensitive_text_input_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_sensitive_text_input_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_sensitive_text_input_tests)
bpr_enable_warnings(test_better_pull_review_qt_sensitive_text_input_tests)

bpr_add_network_isolated_test(
    NAME qt_sensitive_text_input
    COMMAND test_better_pull_review_qt_sensitive_text_input_tests
)
set_tests_properties(
    qt_sensitive_text_input
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "sensitive-text-input-tests:pass"
        TIMEOUT 15
)

bpr_add_executable(
    test_better_pull_review_qt_epoch_comparison_tests
    epoch_comparison_control_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_epoch_comparison_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_epoch_comparison_tests)
bpr_enable_warnings(test_better_pull_review_qt_epoch_comparison_tests)

foreach(bpr_epoch_scale IN ITEMS 100 125 200)
    if(bpr_epoch_scale STREQUAL "100")
        set(bpr_epoch_scale_factor "1.0")
    elseif(bpr_epoch_scale STREQUAL "125")
        set(bpr_epoch_scale_factor "1.25")
    else()
        set(bpr_epoch_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_epoch_comparison_scale_${bpr_epoch_scale}
        COMMAND test_better_pull_review_qt_epoch_comparison_tests
    )
    set_tests_properties(
        qt_epoch_comparison_scale_${bpr_epoch_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_epoch_scale_factor};BPR_QT_EPOCH_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/epoch-comparison-${bpr_epoch_scale}.png"
            PASS_REGULAR_EXPRESSION "epoch-comparison-control-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_pull_request_overview_page_tests
    pull_request_overview_page_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_pull_request_overview_page_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_pull_request_overview_page_tests)
bpr_enable_warnings(test_better_pull_review_qt_pull_request_overview_page_tests)

bpr_add_network_isolated_test(
    NAME qt_pull_request_overview_page
    COMMAND test_better_pull_review_qt_pull_request_overview_page_tests
)
set_tests_properties(
    qt_pull_request_overview_page
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.0"
        PASS_REGULAR_EXPRESSION "pull-request-overview-page-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_comparison_scroll_controller_tests
    comparison_scroll_controller_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_comparison_scroll_controller_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(
    test_better_pull_review_qt_comparison_scroll_controller_tests
)
bpr_enable_warnings(
    test_better_pull_review_qt_comparison_scroll_controller_tests
)

foreach(bpr_scroll_controller_scale IN ITEMS 100 125 200)
    if(bpr_scroll_controller_scale STREQUAL "100")
        set(bpr_scroll_controller_scale_factor "1.0")
    elseif(bpr_scroll_controller_scale STREQUAL "125")
        set(bpr_scroll_controller_scale_factor "1.25")
    else()
        set(bpr_scroll_controller_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_comparison_scroll_controller_scale_${bpr_scroll_controller_scale}
        COMMAND test_better_pull_review_qt_comparison_scroll_controller_tests
    )
    set_tests_properties(
        qt_comparison_scroll_controller_scale_${bpr_scroll_controller_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_scroll_controller_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_scroll_controller_scale_factor}"
            PASS_REGULAR_EXPRESSION "comparison-scroll-controller-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_diff_link_map_tests
    diff_link_map_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_diff_link_map_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_diff_link_map_tests)
bpr_enable_warnings(test_better_pull_review_qt_diff_link_map_tests)

foreach(bpr_diff_link_map_scale IN ITEMS 100 125 200)
    if(bpr_diff_link_map_scale STREQUAL "100")
        set(bpr_diff_link_map_scale_factor "1.0")
    elseif(bpr_diff_link_map_scale STREQUAL "125")
        set(bpr_diff_link_map_scale_factor "1.25")
    else()
        set(bpr_diff_link_map_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_diff_link_map_scale_${bpr_diff_link_map_scale}
        COMMAND test_better_pull_review_qt_diff_link_map_tests
    )
    set_tests_properties(
        qt_diff_link_map_scale_${bpr_diff_link_map_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_diff_link_map_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_diff_link_map_scale_factor};BPR_QT_DIFF_LINK_MAP_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/diff-link-map-${bpr_diff_link_map_scale}.png"
            PASS_REGULAR_EXPRESSION "diff-link-map-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_appearance_settings_panel_tests
    appearance_settings_panel_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_appearance_settings_panel_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_appearance_settings_panel_tests)
bpr_enable_warnings(test_better_pull_review_qt_appearance_settings_panel_tests)

bpr_add_network_isolated_test(
    NAME qt_appearance_settings_panel
    COMMAND test_better_pull_review_qt_appearance_settings_panel_tests
)
set_tests_properties(
    qt_appearance_settings_panel
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "appearance-settings-panel-tests:pass"
        TIMEOUT 120
)

# [[BPR-0790]]. The shared code sample painter, which is what makes the theme
# tiles and the type preview consumers of the comparison panes' painter rather
# than a second renderer of their own.
bpr_add_executable(
    test_better_pull_review_qt_code_sample_painter_tests
    code_sample_painter_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_code_sample_painter_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_code_sample_painter_tests)
bpr_enable_warnings(test_better_pull_review_qt_code_sample_painter_tests)

bpr_add_network_isolated_test(
    NAME qt_code_sample_painter
    COMMAND test_better_pull_review_qt_code_sample_painter_tests
)
set_tests_properties(
    qt_code_sample_painter
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "code-sample-painter-tests:pass"
        TIMEOUT 120
)

# The strip's pixel assertions compare both halves of both System tiles against
# the built-in palettes, which is what RFC-BPR-0034's C2 asks for by name.
bpr_add_executable(
    test_better_pull_review_qt_theme_preview_strip_tests
    theme_preview_strip_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_theme_preview_strip_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_theme_preview_strip_tests)
bpr_enable_warnings(test_better_pull_review_qt_theme_preview_strip_tests)

bpr_add_network_isolated_test(
    NAME qt_theme_preview_strip
    COMMAND test_better_pull_review_qt_theme_preview_strip_tests
)
set_tests_properties(
    qt_theme_preview_strip
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "theme-preview-strip-tests:pass"
        TIMEOUT 120
)

# [[BPR-0790]]. The Colours card's vocabulary and its list. The vocabulary is
# presentation's own: the core's stable names are durable exchange identifiers and
# must never be displayed, so the course checks that none of them is.
bpr_add_executable(
    test_better_pull_review_qt_appearance_role_display_tests
    appearance_role_display_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_appearance_role_display_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_appearance_role_display_tests)
bpr_enable_warnings(test_better_pull_review_qt_appearance_role_display_tests)

bpr_add_network_isolated_test(
    NAME qt_appearance_role_display
    COMMAND test_better_pull_review_qt_appearance_role_display_tests
)
set_tests_properties(
    qt_appearance_role_display
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "appearance-role-display-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_colour_role_list_tests
    colour_role_list_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_colour_role_list_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_colour_role_list_tests)
bpr_enable_warnings(test_better_pull_review_qt_colour_role_list_tests)

bpr_add_network_isolated_test(
    NAME qt_colour_role_list
    COMMAND test_better_pull_review_qt_colour_role_list_tests
)
set_tests_properties(
    qt_colour_role_list
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "colour-role-list-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_type_preview_tests
    type_preview_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_type_preview_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_type_preview_tests)
bpr_enable_warnings(test_better_pull_review_qt_type_preview_tests)

bpr_add_network_isolated_test(
    NAME qt_type_preview
    COMMAND test_better_pull_review_qt_type_preview_tests
)
set_tests_properties(
    qt_type_preview
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "type-preview-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_settings_window_tests
    settings_window_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_settings_window_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_settings_window_tests)
bpr_enable_warnings(test_better_pull_review_qt_settings_window_tests)

bpr_add_network_isolated_test(
    NAME qt_settings_window
    COMMAND test_better_pull_review_qt_settings_window_tests
)
set_tests_properties(
    qt_settings_window
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "settings-window-tests:pass"
        TIMEOUT 120
)

foreach(bpr_settings_scale IN ITEMS 125 150 200)
    math(EXPR bpr_settings_scale_whole "${bpr_settings_scale} / 100")
    math(EXPR bpr_settings_scale_fraction "${bpr_settings_scale} % 100")
    bpr_add_network_isolated_test(
        NAME qt_settings_window_scale_${bpr_settings_scale}
        COMMAND test_better_pull_review_qt_settings_window_tests
    )
    set_tests_properties(
        qt_settings_window_scale_${bpr_settings_scale}
        PROPERTIES
            ENVIRONMENT "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_settings_scale_whole}.${bpr_settings_scale_fraction}"
            PASS_REGULAR_EXPRESSION "settings-window-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_agent_launch_panel_tests
    agent_launch_panel_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_agent_launch_panel_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_agent_launch_panel_tests)
bpr_enable_warnings(test_better_pull_review_qt_agent_launch_panel_tests)

bpr_add_network_isolated_test(
    NAME qt_agent_launch_panel
    COMMAND test_better_pull_review_qt_agent_launch_panel_tests
)
set_tests_properties(
    qt_agent_launch_panel
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "agent-launch-panel-tests:pass"
        TIMEOUT 120
)

# The side chat sits beside the launch panel and is proven the same way: it
# holds no launcher and no process, so an intent is the only thing that can
# leave it.
bpr_add_executable(
    test_better_pull_review_qt_side_chat_panel_tests
    side_chat_panel_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_side_chat_panel_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_side_chat_panel_tests)
bpr_enable_warnings(test_better_pull_review_qt_side_chat_panel_tests)

bpr_add_network_isolated_test(
    NAME qt_side_chat_panel
    COMMAND test_better_pull_review_qt_side_chat_panel_tests
)
set_tests_properties(
    qt_side_chat_panel
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "side-chat-panel-tests:pass"
        TIMEOUT 120
)

# The canary is a real process, so the launcher's guarantee is measured
# against what a child actually receives rather than against the request.
bpr_add_executable(
    test_better_pull_review_agent_canary
    "${PROJECT_SOURCE_DIR}/tests/fixtures/agent/agent_canary_main.cpp"
)
target_compile_features(test_better_pull_review_agent_canary PRIVATE cxx_std_23)
if(WIN32)
    # timeBeginPeriod, so the fixture's millisecond pacing is millisecond
    # pacing rather than a 15.6 ms timer tick per chunk (BPR-0240).
    target_link_libraries(test_better_pull_review_agent_canary PRIVATE winmm)
    # [[BPR-0622]]. The child-side handle probe reads its own handles' object
    # types through ntdll; fixture code only.
    target_link_libraries(test_better_pull_review_agent_canary PRIVATE ntdll)
endif()
bpr_enable_hardening(test_better_pull_review_agent_canary)
bpr_enable_warnings(test_better_pull_review_agent_canary)

bpr_add_executable(
    test_better_pull_review_qt_agent_launch_canary_tests
    agent_launch_canary_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_agent_launch_canary_tests
    PRIVATE
        BetterPullReview::AdapterAgent
        Qt6::Core
        $<$<PLATFORM_ID:Windows>:Advapi32>
)
target_compile_definitions(
    test_better_pull_review_qt_agent_launch_canary_tests
    PRIVATE
        "BPR_AGENT_CANARY_EXECUTABLE=\"$<TARGET_FILE:test_better_pull_review_agent_canary>\""
        BPR_BUILD_TESTING=1
)
add_dependencies(
    test_better_pull_review_qt_agent_launch_canary_tests
    test_better_pull_review_agent_canary
)
bpr_enable_hardening(test_better_pull_review_qt_agent_launch_canary_tests)
bpr_enable_warnings(test_better_pull_review_qt_agent_launch_canary_tests)

bpr_add_network_isolated_test(
    NAME qt_agent_launch_canary
    COMMAND test_better_pull_review_qt_agent_launch_canary_tests
)
set_tests_properties(
    qt_agent_launch_canary
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "agent-launch-canary-tests:pass"
        TIMEOUT 120
)

# [[BPR-0622]]. Windows runs the same course: suspend the supervisor, kill
# the owner, and require the guard to stay held until the job is empty.
if(UNIX OR WIN32)
    bpr_add_network_isolated_test(
        NAME agent_parent_death
        WINDOWS_UNCONTAINED
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/agent_parent_death_tests.py"
            --executable "$<TARGET_FILE:test_better_pull_review_qt_agent_launch_canary_tests>"
            --supervisor "$<TARGET_FILE:tool_better_pull_review_agent_supervisor>"
            --canary "$<TARGET_FILE:test_better_pull_review_agent_canary>"
    )
    # Two separately bounded owner-PID and owner-process-group kill courses.
    set_tests_properties(agent_parent_death PROPERTIES TIMEOUT 120)
endif()

# Linked against both adapters on purpose. Each is proven on its own elsewhere;
# only a target that carries the two together can show the classifier's answer
# arriving in the pane's rendered formats.
bpr_add_executable(
    test_better_pull_review_qt_syntax_pane_composition_tests
    syntax_pane_composition_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_syntax_pane_composition_tests
    PRIVATE
        BetterPullReview::AdapterQt
        BetterPullReview::AdapterSyntax
)
bpr_enable_hardening(test_better_pull_review_qt_syntax_pane_composition_tests)
bpr_enable_warnings(test_better_pull_review_qt_syntax_pane_composition_tests)

bpr_add_network_isolated_test(
    NAME qt_syntax_pane_composition
    COMMAND test_better_pull_review_qt_syntax_pane_composition_tests
)
set_tests_properties(
    qt_syntax_pane_composition
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "syntax-pane-composition-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_diff_workbench_tests
    diff_workbench_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_diff_workbench_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_diff_workbench_tests)
bpr_enable_warnings(test_better_pull_review_qt_diff_workbench_tests)

foreach(bpr_diff_workbench_scale IN ITEMS 100 125 200)
    if(bpr_diff_workbench_scale STREQUAL "100")
        set(bpr_diff_workbench_scale_factor "1.0")
    elseif(bpr_diff_workbench_scale STREQUAL "125")
        set(bpr_diff_workbench_scale_factor "1.25")
    else()
        set(bpr_diff_workbench_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_diff_workbench_scale_${bpr_diff_workbench_scale}
        COMMAND test_better_pull_review_qt_diff_workbench_tests
    )
    set_tests_properties(
        qt_diff_workbench_scale_${bpr_diff_workbench_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_diff_workbench_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_diff_workbench_scale_factor};BPR_QT_DIFF_WORKBENCH_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/diff-workbench-${bpr_diff_workbench_scale}.png"
            PASS_REGULAR_EXPRESSION "diff-workbench-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_workbench_window_tests
    workbench_window_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_workbench_window_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_workbench_window_tests)
bpr_enable_warnings(test_better_pull_review_qt_workbench_window_tests)

foreach(bpr_workbench_window_scale IN ITEMS 100 125 200)
    if(bpr_workbench_window_scale STREQUAL "100")
        set(bpr_workbench_window_scale_factor "1.0")
    elseif(bpr_workbench_window_scale STREQUAL "125")
        set(bpr_workbench_window_scale_factor "1.25")
    else()
        set(bpr_workbench_window_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_workbench_window_scale_${bpr_workbench_window_scale}
        COMMAND test_better_pull_review_qt_workbench_window_tests
    )
    set_tests_properties(
        qt_workbench_window_scale_${bpr_workbench_window_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_workbench_window_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_workbench_window_scale_factor};BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/workbench-window-${bpr_workbench_window_scale}.png"
            PASS_REGULAR_EXPRESSION "workbench-window-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_appearance_profile_tests
    appearance_profile_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_appearance_profile_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_appearance_profile_tests)
bpr_enable_warnings(test_better_pull_review_qt_appearance_profile_tests)

bpr_add_network_isolated_test(
    NAME qt_appearance_profile
    COMMAND test_better_pull_review_qt_appearance_profile_tests
)
set_tests_properties(
    qt_appearance_profile
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "appearance-profile-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_qt_dif_runtime_tests
    dif_runtime_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_dif_runtime_tests
    PRIVATE BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_dif_runtime_tests)
bpr_enable_warnings(test_better_pull_review_qt_dif_runtime_tests)
bpr_add_network_isolated_test(
    NAME qt_dif_runtime
    COMMAND test_better_pull_review_qt_dif_runtime_tests
)
set_tests_properties(qt_dif_runtime PROPERTIES
    ENVIRONMENT "${bpr_qt_github_common_environment}"
    PASS_REGULAR_EXPRESSION "dif-runtime-tests:pass"
    TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_motion_observer_tests
    motion_observer_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_motion_observer_tests
    PRIVATE BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_motion_observer_tests)
bpr_enable_warnings(test_better_pull_review_qt_motion_observer_tests)
bpr_add_network_isolated_test(
    NAME qt_motion_observer
    COMMAND test_better_pull_review_qt_motion_observer_tests
)
set_tests_properties(qt_motion_observer PROPERTIES
    ENVIRONMENT "${bpr_qt_github_common_environment}"
    PASS_REGULAR_EXPRESSION "motion-observer-tests:pass"
    TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_qt_diff_axis_tests
    measured_alignment_axis_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_diff_axis_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_diff_axis_tests)
bpr_enable_warnings(test_better_pull_review_qt_diff_axis_tests)

foreach(bpr_diff_axis_scale IN ITEMS 100 125 200)
    if(bpr_diff_axis_scale STREQUAL "100")
        set(bpr_diff_axis_scale_factor "1.0")
    elseif(bpr_diff_axis_scale STREQUAL "125")
        set(bpr_diff_axis_scale_factor "1.25")
    else()
        set(bpr_diff_axis_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_diff_axis_scale_${bpr_diff_axis_scale}
        COMMAND test_better_pull_review_qt_diff_axis_tests
    )
    set_tests_properties(
        qt_diff_axis_scale_${bpr_diff_axis_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_diff_axis_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_diff_axis_scale_factor};BPR_QT_DIFF_AXIS_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/diff-axis-pane-${bpr_diff_axis_scale}.png"
            PASS_REGULAR_EXPRESSION "measured-alignment-axis-tests:pass"
            TIMEOUT 120
    )
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_review_queue_tests
    review_queue_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_review_queue_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_review_queue_tests)
bpr_enable_warnings(test_better_pull_review_qt_review_queue_tests)

bpr_add_network_isolated_test(
    NAME qt_review_queue
    COMMAND test_better_pull_review_qt_review_queue_tests
)
set_tests_properties(
    qt_review_queue
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
)

bpr_add_executable(
    test_better_pull_review_qt_review_draft_editor_tests
    review_draft_editor_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_review_draft_editor_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_review_draft_editor_tests)
bpr_enable_warnings(test_better_pull_review_qt_review_draft_editor_tests)

bpr_add_network_isolated_test(
    NAME qt_review_draft_editor
    COMMAND test_better_pull_review_qt_review_draft_editor_tests
)
set_tests_properties(
    qt_review_draft_editor
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "review-draft-editor-tests:pass"
)

# The finding queue sits beside the draft editor and is proven the same way:
# it holds no provider, no endpoint, and no store, so an intent naming one
# exact finding is the only thing that can leave it.
bpr_add_executable(
    test_better_pull_review_qt_finding_queue_view_tests
    finding_queue_view_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_finding_queue_view_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_finding_queue_view_tests)
bpr_enable_warnings(test_better_pull_review_qt_finding_queue_view_tests)

bpr_add_network_isolated_test(
    NAME qt_finding_queue_view
    COMMAND test_better_pull_review_qt_finding_queue_view_tests
)
set_tests_properties(
    qt_finding_queue_view
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "finding-queue-view-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(
    test_better_pull_review_qt_defect_card_tests
    defect_card_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_defect_card_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_defect_card_tests)
bpr_enable_warnings(test_better_pull_review_qt_defect_card_tests)

bpr_add_network_isolated_test(
    NAME qt_defect_card
    COMMAND test_better_pull_review_qt_defect_card_tests
)
set_tests_properties(
    qt_defect_card
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
)

bpr_add_executable(
    test_better_pull_review_qt_review_landing_model_tests
    review_landing_model_tests.cpp
)

target_link_libraries(
    test_better_pull_review_qt_review_landing_model_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_review_landing_model_tests)
bpr_enable_warnings(test_better_pull_review_qt_review_landing_model_tests)

bpr_add_network_isolated_test(
    NAME qt_review_landing_model
    COMMAND test_better_pull_review_qt_review_landing_model_tests
)
set_tests_properties(
    qt_review_landing_model
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
)

# [[BPR-0776]]. Home's state panels, one row anatomy, sticky headers, empty
# states and the Resume row action, at the three registered scales.
bpr_add_executable(test_better_pull_review_qt_home_list_tests home_list_tests.cpp)
target_link_libraries(test_better_pull_review_qt_home_list_tests PRIVATE BetterPullReview::AdapterQt)
bpr_enable_hardening(test_better_pull_review_qt_home_list_tests)
bpr_enable_warnings(test_better_pull_review_qt_home_list_tests)
foreach(bpr_home_list_scale IN ITEMS 100 125 200)
    if(bpr_home_list_scale STREQUAL "100")
        set(bpr_home_list_scale_factor "1.0")
    elseif(bpr_home_list_scale STREQUAL "125")
        set(bpr_home_list_scale_factor "1.25")
    else()
        set(bpr_home_list_scale_factor "2.0")
    endif()
    bpr_add_network_isolated_test(
        NAME qt_home_list_scale_${bpr_home_list_scale}
        COMMAND test_better_pull_review_qt_home_list_tests "${CMAKE_CURRENT_BINARY_DIR}/home-list-${bpr_home_list_scale}.png"
    )
    set_tests_properties(
        qt_home_list_scale_${bpr_home_list_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_home_list_scale_factor};BPR_EXPECTED_QT_SCALE=${bpr_home_list_scale_factor}"
            PASS_REGULAR_EXPRESSION "home-list-tests:pass"
            TIMEOUT 120
    )
endforeach()

# PR-scoped editor mechanics, independent of provider effects.
bpr_add_executable(test_better_pull_review_qt_review_submission_editor_tests review_submission_editor_tests.cpp)
target_link_libraries(test_better_pull_review_qt_review_submission_editor_tests PRIVATE BetterPullReview::AdapterQt)
bpr_enable_hardening(test_better_pull_review_qt_review_submission_editor_tests)
bpr_enable_warnings(test_better_pull_review_qt_review_submission_editor_tests)
foreach(scale IN ITEMS 100 125 200)
    math(EXPR scale_integer "${scale} / 100")
    if(scale EQUAL 125)
        set(scale_factor "1.25")
    else()
        set(scale_factor "${scale_integer}")
    endif()
    bpr_add_network_isolated_test(NAME qt_review_submission_editor_scale_${scale}
        COMMAND test_better_pull_review_qt_review_submission_editor_tests "${CMAKE_CURRENT_BINARY_DIR}/review-submission-editor-${scale}.png")
    set_tests_properties(qt_review_submission_editor_scale_${scale} PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${scale_factor}"
        PASS_REGULAR_EXPRESSION "review-submission-editor:pass")
endforeach()

bpr_add_executable(
    test_better_pull_review_qt_settings_effect_boundary_tests
    settings_effect_boundary_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_settings_effect_boundary_tests
    PRIVATE BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_settings_effect_boundary_tests)
bpr_enable_warnings(test_better_pull_review_qt_settings_effect_boundary_tests)
bpr_add_network_isolated_test(
    NAME qt_settings_effect_boundary
    COMMAND test_better_pull_review_qt_settings_effect_boundary_tests
)
set_tests_properties(
    qt_settings_effect_boundary
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "settings-effect-boundary-tests:pass"
        TIMEOUT 120
)

bpr_add_executable(test_better_pull_review_qt_profile_management_window_tests profile_management_window_tests.cpp)
target_link_libraries(test_better_pull_review_qt_profile_management_window_tests PRIVATE BetterPullReview::AdapterQt)
bpr_enable_hardening(test_better_pull_review_qt_profile_management_window_tests)
bpr_enable_warnings(test_better_pull_review_qt_profile_management_window_tests)
bpr_add_network_isolated_test(NAME qt_profile_management_window COMMAND test_better_pull_review_qt_profile_management_window_tests)
set_tests_properties(qt_profile_management_window PROPERTIES ENVIRONMENT "${bpr_qt_github_common_environment}" TIMEOUT 60)

# [[BPR-0779]]. RFC-BPR-0032 A8, RFC-BPR-0033 B5 and RFC-BPR-0034 C6's
# four-appearance, four-scale rendered and reachable course for the Accounts
# page, Home, the Profiles sheet and the Appearance page.
#
# Built rather than extended. The three criteria each speak of extending "the
# existing four-appearance, four-scale capture course"; there was none. The four
# appearances were iterated inside individual translation units, and the
# registered scale variants were 100, 125 and 200 everywhere except
# qt_settings_window, which alone carried 150. This is the first registration at
# all four scales for these surfaces.
#
# Captures are written only when BPR_SURFACE_CAPTURE_DIR names a directory, which
# is the gating convention BPR_SETTINGS_RENDER_ROOT and BPR_HOME_CAPTURE_DIR
# already use: an ordinary run writes nothing and stays fast, while every
# composition assertion still runs.
bpr_add_executable(
    test_better_pull_review_qt_surface_capture_course_tests
    surface_capture_course_tests.cpp
)
target_link_libraries(
    test_better_pull_review_qt_surface_capture_course_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(test_better_pull_review_qt_surface_capture_course_tests)
bpr_enable_warnings(test_better_pull_review_qt_surface_capture_course_tests)

foreach(bpr_surface_capture_scale IN ITEMS 100 125 150 200)
    math(EXPR bpr_surface_capture_whole "${bpr_surface_capture_scale} / 100")
    math(EXPR bpr_surface_capture_fraction "${bpr_surface_capture_scale} % 100")
    set(bpr_surface_capture_factor
        "${bpr_surface_capture_whole}.${bpr_surface_capture_fraction}"
    )
    bpr_add_network_isolated_test(
        NAME qt_surface_capture_course_scale_${bpr_surface_capture_scale}
        COMMAND test_better_pull_review_qt_surface_capture_course_tests
    )
    set_tests_properties(
        qt_surface_capture_course_scale_${bpr_surface_capture_scale}
        PROPERTIES
            ENVIRONMENT
                "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=${bpr_surface_capture_factor};BPR_EXPECTED_QT_SCALE=${bpr_surface_capture_factor}"
            PASS_REGULAR_EXPRESSION "surface-capture-course-tests:pass"
            TIMEOUT 600
    )
endforeach()
