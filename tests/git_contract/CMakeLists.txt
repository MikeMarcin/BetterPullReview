find_package(Git 2.39 REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
find_package(Threads REQUIRED)

add_test(
    NAME epoch_history_fixture_contract
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_epoch_history_fixture.py"
        --git
        "${GIT_EXECUTABLE}"
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

add_executable(
    better_pull_review_git_process_contract_tests
    git_process_contract_tests.cpp
    ../../src/adapters/git/src/installed_git_process.cpp
)
target_include_directories(
    better_pull_review_git_process_contract_tests
    PRIVATE
        ../../src/adapters/git/include
)
target_link_libraries(
    better_pull_review_git_process_contract_tests
    PRIVATE
        BetterPullReview::Application
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    better_pull_review_git_process_contract_tests
    PRIVATE
        BPR_INSTALLED_GIT_PROCESS_FAULT_INJECTION=1
        BPR_TEST_GIT_EXECUTABLE="${GIT_EXECUTABLE}"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_git_process_contract_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
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
        TIMEOUT 180
)

add_executable(
    better_pull_review_git_cache_contract_tests
    git_cache_contract_tests.cpp
    ../../src/adapters/git/src/installed_git_cache.cpp
    ../../src/adapters/git/src/installed_git_process.cpp
)
target_include_directories(
    better_pull_review_git_cache_contract_tests
    PRIVATE
        ../../src/adapters/git/include
)
target_link_libraries(
    better_pull_review_git_cache_contract_tests
    PRIVATE
        BetterPullReview::Application
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    better_pull_review_git_cache_contract_tests
    PRIVATE
        BPR_INSTALLED_GIT_CACHE_FAULT_INJECTION=1
        BPR_TEST_GIT_EXECUTABLE="${GIT_EXECUTABLE}"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_git_cache_contract_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    better_pull_review_git_cache_contract_tests
)
bpr_enable_warnings(
    better_pull_review_git_cache_contract_tests
)

add_test(
    NAME git_cache_contract
    COMMAND better_pull_review_git_cache_contract_tests
)
set_tests_properties(
    git_cache_contract
    PROPERTIES
        TIMEOUT 600
)

add_executable(
    better_pull_review_epoch_maintenance_integration_tests
    epoch_maintenance_integration_tests.cpp
)
target_link_libraries(
    better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BetterPullReview::Application
        BetterPullReview::AdapterGit
        BetterPullReview::AdapterSQLite
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BPR_TEST_GIT_EXECUTABLE="${GIT_EXECUTABLE}"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_epoch_maintenance_integration_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    better_pull_review_epoch_maintenance_integration_tests
)
bpr_enable_warnings(
    better_pull_review_epoch_maintenance_integration_tests
)

add_test(
    NAME epoch_maintenance_integration
    COMMAND better_pull_review_epoch_maintenance_integration_tests
)
set_tests_properties(
    epoch_maintenance_integration
    PROPERTIES
        TIMEOUT 90
)
