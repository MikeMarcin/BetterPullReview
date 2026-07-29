add_executable(
    better_pull_review_application_tests
    host_tests.cpp
)

target_link_libraries(
    better_pull_review_application_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(better_pull_review_application_tests)
bpr_enable_warnings(better_pull_review_application_tests)

add_test(NAME application_host COMMAND better_pull_review_application_tests)
