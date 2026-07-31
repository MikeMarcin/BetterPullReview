find_package(Git 2.39 REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
find_package(Threads REQUIRED)

add_executable(
    better_pull_review_git_process_contract_tests
    git_process_contract_tests.cpp
)
target_link_libraries(
    better_pull_review_git_process_contract_tests
    PRIVATE
        BetterPullReview::AdapterGit
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    better_pull_review_git_process_contract_tests
    PRIVATE
        BPR_TEST_GIT_EXECUTABLE="${GIT_EXECUTABLE}"
)
bpr_enable_hardening(
    better_pull_review_git_process_contract_tests
)
bpr_enable_warnings(
    better_pull_review_git_process_contract_tests
)

add_test(
    NAME git_process_contract
    COMMAND better_pull_review_git_process_contract_tests
)
set_tests_properties(
    git_process_contract
    PROPERTIES
        TIMEOUT 90
)
