find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_executable(
    better_pull_review_github_contract_tests
    github_read_contract_tests.cpp
)

target_link_libraries(
    better_pull_review_github_contract_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    better_pull_review_github_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_link_libraries(
    better_pull_review_github_contract_tests
    PRIVATE
        Qt6::Core
)
target_compile_definitions(
    better_pull_review_github_contract_tests
    PRIVATE
        BPR_GITHUB_CONTRACT_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1"
)
bpr_enable_hardening(better_pull_review_github_contract_tests)
bpr_enable_warnings(better_pull_review_github_contract_tests)

add_test(
    NAME github_read_contract
    COMMAND better_pull_review_github_contract_tests
)
