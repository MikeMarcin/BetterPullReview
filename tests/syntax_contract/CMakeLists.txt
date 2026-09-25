bpr_add_executable(
    test_better_pull_review_syntax_classification_contract_tests
    syntax_classification_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_syntax_classification_contract_tests
    PRIVATE
        BetterPullReview::AdapterSyntax
        KF6::SyntaxHighlighting
        Qt6::Core
)
if(WIN32)
    # [[BPR-1126]]. On Windows the canary's control root is made by the SQLite
    # contracts' scratch helper, with the platform target's owner-private
    # descriptor, and checked by the stores' own rule.
    target_link_libraries(
        test_better_pull_review_syntax_classification_contract_tests
        PRIVATE
            BetterPullReview::AdapterSQLite
            BetterPullReview::Platform
    )
endif()
target_compile_definitions(
    test_better_pull_review_syntax_classification_contract_tests
    PRIVATE
        BPR_SYNTAX_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/syntax"
)
bpr_enable_hardening(
    test_better_pull_review_syntax_classification_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_syntax_classification_contract_tests
)

bpr_add_network_isolated_test(
    NAME syntax_classification_contract
    COMMAND test_better_pull_review_syntax_classification_contract_tests
)
set_tests_properties(
    syntax_classification_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "syntax-classification-contract-tests:pass"
        TIMEOUT 120
)
