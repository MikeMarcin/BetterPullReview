add_executable(
    better_pull_review_qt_tests
    foundation_window_tests.cpp
)

target_link_libraries(
    better_pull_review_qt_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(better_pull_review_qt_tests)
bpr_enable_warnings(better_pull_review_qt_tests)

add_test(NAME qt_foundation_window COMMAND better_pull_review_qt_tests)
set_tests_properties(
    qt_foundation_window
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
)

add_executable(
    better_pull_review_qt_github_tests
    github_window_tests.cpp
)

target_link_libraries(
    better_pull_review_qt_github_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(better_pull_review_qt_github_tests)
bpr_enable_warnings(better_pull_review_qt_github_tests)

set(bpr_qt_github_common_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    # Qt's basic style avoids a Fusion cached-painter fault in fractional-scale
    # offscreen rendering. Native-style UX validation remains a separate gate.
    list(APPEND bpr_qt_github_common_environment "QT_STYLE_OVERRIDE=Windows")
endif()

add_test(
    NAME qt_github_window_scale_100
    COMMAND better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_100
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.0;BPR_EXPECTED_QT_SCALE=1.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-100.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-100.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-100.png"
)

add_test(
    NAME qt_github_window_scale_125
    COMMAND better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_125
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=1.25;BPR_EXPECTED_QT_SCALE=1.25;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-125.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-125.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-125.png"
)

add_test(
    NAME qt_github_window_scale_200
    COMMAND better_pull_review_qt_github_tests
)
set_tests_properties(
    qt_github_window_scale_200
    PROPERTIES
        ENVIRONMENT
            "${bpr_qt_github_common_environment};QT_SCALE_FACTOR=2.0;BPR_EXPECTED_QT_SCALE=2.0;BPR_QT_MIN_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-200.png;BPR_QT_MIN_LOWER_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-minimum-lower-200.png;BPR_QT_RENDER_PATH=${CMAKE_CURRENT_BINARY_DIR}/github-window-200.png"
)

add_executable(
    better_pull_review_qt_sensitive_text_input_tests
    sensitive_text_input_tests.cpp
)

target_link_libraries(
    better_pull_review_qt_sensitive_text_input_tests
    PRIVATE
        BetterPullReview::AdapterQt
)
bpr_enable_hardening(better_pull_review_qt_sensitive_text_input_tests)
bpr_enable_warnings(better_pull_review_qt_sensitive_text_input_tests)

add_test(
    NAME qt_sensitive_text_input
    COMMAND better_pull_review_qt_sensitive_text_input_tests
)
set_tests_properties(
    qt_sensitive_text_input
    PROPERTIES
        ENVIRONMENT "${bpr_qt_github_common_environment}"
        PASS_REGULAR_EXPRESSION "sensitive-text-input-tests:pass"
        TIMEOUT 15
)
