add_executable(
    better_pull_review_product_contract_tests
    product_contract_tests.cpp
)

target_link_libraries(
    better_pull_review_product_contract_tests
    PRIVATE
        BetterPullReview::Core
)
bpr_enable_hardening(better_pull_review_product_contract_tests)
bpr_enable_warnings(better_pull_review_product_contract_tests)

add_test(
    NAME product_contract
    COMMAND better_pull_review_product_contract_tests
)
