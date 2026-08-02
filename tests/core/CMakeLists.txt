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

add_executable(
    better_pull_review_github_core_tests
    github_policy_tests.cpp
)

target_link_libraries(
    better_pull_review_github_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_github_core_tests)
bpr_enable_warnings(better_pull_review_github_core_tests)

add_test(
    NAME core_github_policy
    COMMAND better_pull_review_github_core_tests
)

add_executable(
    better_pull_review_epoch_core_tests
    epoch_policy_tests.cpp
)

target_link_libraries(
    better_pull_review_epoch_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_epoch_core_tests)
bpr_enable_warnings(better_pull_review_epoch_core_tests)

add_test(
    NAME core_epoch_policy
    COMMAND better_pull_review_epoch_core_tests
)

add_executable(
    better_pull_review_diff_core_tests
    diff_model_tests.cpp
)

target_link_libraries(
    better_pull_review_diff_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_diff_core_tests)
bpr_enable_warnings(better_pull_review_diff_core_tests)

add_test(
    NAME core_diff_model
    COMMAND better_pull_review_diff_core_tests
)

add_executable(
    better_pull_review_diff_compute_core_tests
    diff_compute_tests.cpp
)

target_link_libraries(
    better_pull_review_diff_compute_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_diff_compute_core_tests)
bpr_enable_warnings(better_pull_review_diff_compute_core_tests)

add_test(
    NAME core_diff_compute
    COMMAND better_pull_review_diff_compute_core_tests
)

add_executable(
    better_pull_review_diff_semantics_core_tests
    diff_semantics_tests.cpp
)

target_link_libraries(
    better_pull_review_diff_semantics_core_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_diff_semantics_core_tests)
bpr_enable_warnings(better_pull_review_diff_semantics_core_tests)

add_test(
    NAME core_diff_semantics
    COMMAND better_pull_review_diff_semantics_core_tests
)
