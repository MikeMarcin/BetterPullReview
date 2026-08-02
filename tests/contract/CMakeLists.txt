bpr_add_executable(
    test_better_pull_review_product_contract_tests
    product_contract_tests.cpp
)

target_link_libraries(
    test_better_pull_review_product_contract_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(test_better_pull_review_product_contract_tests)
bpr_enable_warnings(test_better_pull_review_product_contract_tests)

add_test(
    NAME product_contract
    COMMAND test_better_pull_review_product_contract_tests
)
