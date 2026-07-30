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

add_executable(
    better_pull_review_github_host_profile_tests
    github_host_profile_tests.cpp
)
target_link_libraries(
    better_pull_review_github_host_profile_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    better_pull_review_github_host_profile_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(better_pull_review_github_host_profile_tests)
bpr_enable_warnings(better_pull_review_github_host_profile_tests)

add_test(
    NAME github_host_profile
    COMMAND better_pull_review_github_host_profile_tests
)

add_executable(
    better_pull_review_github_capability_probe_tests
    github_capability_probe_tests.cpp
)
target_link_libraries(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        Qt6::Core
)
target_include_directories(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        BPR_GITHUB_CAPABILITY_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1"
)
bpr_enable_hardening(
    better_pull_review_github_capability_probe_tests
)
bpr_enable_warnings(
    better_pull_review_github_capability_probe_tests
)

add_test(
    NAME github_capability_probe
    COMMAND better_pull_review_github_capability_probe_tests
)
