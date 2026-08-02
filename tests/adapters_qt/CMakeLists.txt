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

add_test(NAME qt_foundation_window COMMAND test_better_pull_review_qt_tests)
set_tests_properties(
    qt_foundation_window
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
)

bpr_add_executable(
    test_better_pull_review_qt_github_tests
    github_window_tests.cpp
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

add_test(
    NAME qt_github_window_scale_100
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_100
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.0;BPR_EXPECTED_QT_SCALE=1.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-100.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-100.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-100.png"
        TIMEOUT 120
)

add_test(
    NAME qt_github_window_scale_125
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_125
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.25;BPR_EXPECTED_QT_SCALE=1.25;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-125.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-125.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-125.png"
        TIMEOUT 120
)

add_test(
    NAME qt_github_window_scale_200
    COMMAND test_better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_200
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=2.0;BPR_EXPECTED_QT_SCALE=2.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-200.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-200.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-200.png"
        TIMEOUT 120
)

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

add_test(
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

add_test(
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
    add_test(
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
