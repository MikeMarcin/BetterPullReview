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
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
