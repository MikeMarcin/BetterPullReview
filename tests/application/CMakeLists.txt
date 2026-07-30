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

add_executable(
    better_pull_review_application_github_contract_tests
    github_contract_tests.cpp
)

target_link_libraries(
    better_pull_review_application_github_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(better_pull_review_application_github_contract_tests)
bpr_enable_warnings(better_pull_review_application_github_contract_tests)

add_test(
    NAME application_github_contract
    COMMAND better_pull_review_application_github_contract_tests
)

add_executable(
    better_pull_review_application_github_capability_cache_tests
    github_capability_cache_tests.cpp
)

target_link_libraries(
    better_pull_review_application_github_capability_cache_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    better_pull_review_application_github_capability_cache_tests
)
bpr_enable_warnings(
    better_pull_review_application_github_capability_cache_tests
)

add_test(
    NAME application_github_capability_cache
    COMMAND better_pull_review_application_github_capability_cache_tests
)
