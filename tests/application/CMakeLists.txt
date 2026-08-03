bpr_add_executable(
    test_better_pull_review_application_tests
    host_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_tests)
bpr_enable_warnings(test_better_pull_review_application_tests)

add_test(NAME application_host COMMAND test_better_pull_review_application_tests)

bpr_add_executable(
    test_better_pull_review_application_github_host_tests
    github_application_host_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_github_host_tests)
bpr_enable_warnings(test_better_pull_review_application_github_host_tests)

add_test(
    NAME application_github_host
    COMMAND test_better_pull_review_application_github_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_contract_tests
    github_contract_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_github_contract_tests)
bpr_enable_warnings(test_better_pull_review_application_github_contract_tests)

add_test(
    NAME application_github_contract
    COMMAND test_better_pull_review_application_github_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_capability_cache_tests
    github_capability_cache_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_capability_cache_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_github_capability_cache_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_github_capability_cache_tests
)

add_test(
    NAME application_github_capability_cache
    COMMAND test_better_pull_review_application_github_capability_cache_tests
)

bpr_add_executable(
    test_better_pull_review_application_github_credential_vault_tests
    github_credential_vault_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_github_credential_vault_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_github_credential_vault_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_github_credential_vault_tests
)

add_test(
    NAME application_github_credential_vault
    COMMAND test_better_pull_review_application_github_credential_vault_tests
)

bpr_add_executable(
    test_better_pull_review_application_git_protocol_v2_tests
    git_protocol_v2_tests.cpp
)

target_link_libraries(
    test_better_pull_review_application_git_protocol_v2_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_git_protocol_v2_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_git_protocol_v2_tests
)

add_test(
    NAME application_git_protocol_v2
    COMMAND test_better_pull_review_application_git_protocol_v2_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_catalog_contract_tests
    epoch_catalog_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_catalog_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_catalog_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_catalog_contract_tests
)

add_test(
    NAME application_epoch_catalog_contract
    COMMAND test_better_pull_review_application_epoch_catalog_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_contract_tests
    epoch_application_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_contract_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_contract_tests
)

add_test(
    NAME application_epoch_contract
    COMMAND test_better_pull_review_application_epoch_contract_tests
)

bpr_add_executable(
    test_better_pull_review_application_epoch_host_tests
    epoch_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_epoch_host_tests
    PRIVATE
        BetterPullReview::Application
)
target_compile_definitions(
    test_better_pull_review_application_epoch_host_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_application_epoch_host_tests
)
bpr_enable_warnings(
    test_better_pull_review_application_epoch_host_tests
)

add_test(
    NAME application_epoch_host
    COMMAND test_better_pull_review_application_epoch_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_diff_host_tests
    diff_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_diff_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_diff_host_tests)
bpr_enable_warnings(test_better_pull_review_application_diff_host_tests)

add_test(
    NAME application_diff_host
    COMMAND test_better_pull_review_application_diff_host_tests
)

bpr_add_executable(
    test_better_pull_review_application_workbench_host_tests
    workbench_application_host_tests.cpp
)
target_link_libraries(
    test_better_pull_review_application_workbench_host_tests
    PRIVATE
        BetterPullReview::Application
)
bpr_enable_hardening(test_better_pull_review_application_workbench_host_tests)
bpr_enable_warnings(test_better_pull_review_application_workbench_host_tests)

add_test(
    NAME application_workbench_host
    COMMAND test_better_pull_review_application_workbench_host_tests
)
