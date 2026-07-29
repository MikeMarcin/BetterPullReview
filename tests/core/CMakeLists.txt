add_executable(
    better_pull_review_core_tests
    foundation_tests.cpp
)

target_link_libraries(
    better_pull_review_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_core_tests)
bpr_enable_warnings(better_pull_review_core_tests)

add_test(NAME core_foundation COMMAND better_pull_review_core_tests)
