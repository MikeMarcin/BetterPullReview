add_test(
    NAME epoch_history_fixture_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_epoch_history_fixture.py"
        --git
        "$<TARGET_FILE:Git::Git>"
        --manifest
        "${PROJECT_SOURCE_DIR}/tests/fixtures/git/epoch-history/v1/manifest.json"
        --source
        "${PROJECT_SOURCE_DIR}/src/app/src/github_public_path_fixture.cpp"
)
set_tests_properties(
    epoch_history_fixture_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "EPOCH_HISTORY_FIXTURE_OK"
        TIMEOUT 30
)

bpr_add_executable(
    test_better_pull_review_git_process_contract_tests
    git_process_contract_tests.cpp
    ../../src/adapters/git/src/installed_git_process.cpp
)
target_include_directories(
    test_better_pull_review_git_process_contract_tests
    PRIVATE
        ../../src/adapters/git/include
)
target_link_libraries(
    test_better_pull_review_git_process_contract_tests
    PRIVATE
        BetterPullReview::Application
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    test_better_pull_review_git_process_contract_tests
    PRIVATE
        BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION=1
        BPR_TEST_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        test_better_pull_review_git_process_contract_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_git_process_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_git_process_contract_tests
)

add_test(
    NAME git_process_contract
    COMMAND test_better_pull_review_git_process_contract_tests
)
set_tests_properties(
    git_process_contract
    PROPERTIES
        TIMEOUT 180
)

bpr_add_executable(
    test_better_pull_review_git_cache_contract_tests
    git_cache_contract_tests.cpp
    ../../src/adapters/git/src/installed_git_cache.cpp
    ../../src/adapters/git/src/installed_git_process.cpp
)
target_include_directories(
    test_better_pull_review_git_cache_contract_tests
    PRIVATE
        ../../src/adapters/git/include
)
target_link_libraries(
    test_better_pull_review_git_cache_contract_tests
    PRIVATE
        BetterPullReview::Application
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    test_better_pull_review_git_cache_contract_tests
    PRIVATE
        BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION=1
        BPR_TEST_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        test_better_pull_review_git_cache_contract_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_git_cache_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_git_cache_contract_tests
)

add_test(
    NAME git_cache_contract
    COMMAND test_better_pull_review_git_cache_contract_tests
)
set_tests_properties(
    git_cache_contract
    PROPERTIES
        TIMEOUT 600
)

bpr_add_executable(
    test_better_pull_review_epoch_maintenance_integration_tests
    epoch_maintenance_integration_tests.cpp
)
target_link_libraries(
    test_better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BetterPullReview::AdapterGit
        BetterPullReview::AdapterSQLite
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    test_better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BPR_TEST_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        test_better_pull_review_epoch_maintenance_integration_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_epoch_maintenance_integration_tests
)
bpr_enable_warnings(
    test_better_pull_review_epoch_maintenance_integration_tests
)

add_test(
    NAME epoch_maintenance_integration
    COMMAND test_better_pull_review_epoch_maintenance_integration_tests
)
set_tests_properties(
    epoch_maintenance_integration
    PROPERTIES
        TIMEOUT 90
)
