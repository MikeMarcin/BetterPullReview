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
    better_pull_review_application_github_host_tests
    github_application_host_tests.cpp
)

target_link_libraries(
    better_pull_review_application_github_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(better_pull_review_application_github_host_tests)
bpr_enable_warnings(better_pull_review_application_github_host_tests)

add_test(
    NAME application_github_host
    COMMAND better_pull_review_application_github_host_tests
)

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

add_executable(
    better_pull_review_application_github_credential_vault_tests
    github_credential_vault_tests.cpp
)

target_link_libraries(
    better_pull_review_application_github_credential_vault_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    better_pull_review_application_github_credential_vault_tests
)
bpr_enable_warnings(
    better_pull_review_application_github_credential_vault_tests
)

add_test(
    NAME application_github_credential_vault
    COMMAND better_pull_review_application_github_credential_vault_tests
)

add_executable(
    better_pull_review_application_git_protocol_v2_tests
    git_protocol_v2_tests.cpp
)

target_link_libraries(
    better_pull_review_application_git_protocol_v2_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    better_pull_review_application_git_protocol_v2_tests
)
bpr_enable_warnings(
    better_pull_review_application_git_protocol_v2_tests
)

add_test(
    NAME application_git_protocol_v2
    COMMAND better_pull_review_application_git_protocol_v2_tests
)

add_executable(
    better_pull_review_application_epoch_catalog_contract_tests
    epoch_catalog_contract_tests.cpp
)
target_link_libraries(
    better_pull_review_application_epoch_catalog_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    better_pull_review_application_epoch_catalog_contract_tests
)
bpr_enable_warnings(
    better_pull_review_application_epoch_catalog_contract_tests
)

add_test(
    NAME application_epoch_catalog_contract
    COMMAND better_pull_review_application_epoch_catalog_contract_tests
)
