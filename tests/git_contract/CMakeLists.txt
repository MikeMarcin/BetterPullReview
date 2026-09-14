bpr_add_network_isolated_test(
    NAME epoch_history_fixture_contract
    WINDOWS_UNCONTAINED
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

bpr_add_network_isolated_test(
    NAME profile_window_restoration_fixture_contract
    WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_profile_window_restoration_fixture.py"
        --git "$<TARGET_FILE:Git::Git>"
)
set_tests_properties(profile_window_restoration_fixture_contract PROPERTIES
    PASS_REGULAR_EXPRESSION "PROFILE_WINDOW_RESTORATION_FIXTURE_OK" TIMEOUT 30)

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
        glaze::glaze
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

bpr_add_network_isolated_test(
    NAME git_process_contract
    WINDOWS_UNCONTAINED
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
        glaze::glaze
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

bpr_add_network_isolated_test(
    NAME git_cache_contract
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_git_cache_contract_tests
)
set_tests_properties(
    git_cache_contract
    PROPERTIES
        TIMEOUT 600
)

bpr_add_network_isolated_test(
    NAME git_sparse_cache_contract
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_git_cache_contract_tests --sparse
)
set_tests_properties(git_sparse_cache_contract PROPERTIES TIMEOUT 180)

bpr_add_executable(test_better_pull_review_migrated_binding_agent migrated_binding_agent_fixture.cpp)
target_link_libraries(test_better_pull_review_migrated_binding_agent PRIVATE BetterPullReview::AdapterMcp)
bpr_enable_hardening(test_better_pull_review_migrated_binding_agent)
bpr_enable_warnings(test_better_pull_review_migrated_binding_agent)

bpr_add_executable(
    test_better_pull_review_epoch_maintenance_integration_tests
    epoch_maintenance_integration_tests.cpp
)
target_link_libraries(
    test_better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BetterPullReview::AdapterGit
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
        BetterPullReview::AdapterMcp
        BetterPullReview::AdapterAgent
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    test_better_pull_review_epoch_maintenance_integration_tests
    PRIVATE
        BPR_SQLITE_TESTING=1
        BPR_MIGRATED_BINDING_AGENT_EXECUTABLE="$<TARGET_FILE:test_better_pull_review_migrated_binding_agent>"
        BPR_TEST_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
add_dependencies(test_better_pull_review_epoch_maintenance_integration_tests test_better_pull_review_migrated_binding_agent)
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

bpr_add_network_isolated_test(
    NAME epoch_maintenance_integration
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_epoch_maintenance_integration_tests
)
set_tests_properties(
    epoch_maintenance_integration
    PROPERTIES
        TIMEOUT 90
)

bpr_add_network_isolated_test(
    NAME epoch_kill_recovery
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/epoch_kill_recovery_tests.py"
        --executable "$<TARGET_FILE:test_better_pull_review_epoch_maintenance_integration_tests>"
        --git "$<TARGET_FILE:Git::Git>"
)
set_tests_properties(epoch_kill_recovery PROPERTIES TIMEOUT 180)

bpr_add_network_isolated_test(
    NAME epoch_transaction_kill_recovery
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/epoch_transaction_kill_recovery_tests.py"
        --executable "$<TARGET_FILE:test_better_pull_review_epoch_maintenance_integration_tests>"
        --git "$<TARGET_FILE:Git::Git>"
)
set_tests_properties(epoch_transaction_kill_recovery PROPERTIES TIMEOUT 300)


if(WIN32)
    # Both contracts launch this executable again as their fake Git, through
    # the adapter's own environment, whose PATH is the executable's directory
    # and nothing else: the shared Qt it links must sit beside it
    # (BPR-0437).
    foreach(bpr_fake_git_target IN ITEMS
        test_better_pull_review_git_process_contract_tests
        test_better_pull_review_git_cache_contract_tests
    )
        add_custom_command(
            TARGET "${bpr_fake_git_target}"
            POST_BUILD
            COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${bpr_fake_git_target}>
                $<TARGET_FILE_DIR:${bpr_fake_git_target}>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endforeach()
endif()

bpr_add_executable(
    test_better_pull_review_git_anonymous_public_read_tests
    anonymous_public_read_tests.cpp
    ../../src/adapters/git/src/installed_git_cache.cpp
    ../../src/adapters/git/src/installed_git_process.cpp
)
target_include_directories(
    test_better_pull_review_git_anonymous_public_read_tests
    PRIVATE
        ../../src/adapters/git/include
)
target_link_libraries(
    test_better_pull_review_git_anonymous_public_read_tests
    PRIVATE
        BetterPullReview::Application
        glaze::glaze
        Qt6::Core
        Threads::Threads
)
target_compile_definitions(
    test_better_pull_review_git_anonymous_public_read_tests
    PRIVATE
        BPR_TEST_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        test_better_pull_review_git_anonymous_public_read_tests
        PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_git_anonymous_public_read_tests
)
bpr_enable_warnings(
    test_better_pull_review_git_anonymous_public_read_tests
)

bpr_add_network_isolated_test(
    NAME git_anonymous_public_read
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_git_anonymous_public_read_tests
)
set_tests_properties(
    git_anonymous_public_read
    PROPERTIES
        PASS_REGULAR_EXPRESSION "git-anonymous-public-read-tests:pass"
        TIMEOUT 180
)

if(UNIX)
    bpr_add_executable(
        test_better_pull_review_agent_workspace_ownership_tests
        agent_workspace_ownership_tests.cpp
        ../../src/adapters/git/src/installed_git_process.cpp
    )
    target_include_directories(
        test_better_pull_review_agent_workspace_ownership_tests
        PRIVATE ../../src/adapters/git/include
    )
    target_link_libraries(
        test_better_pull_review_agent_workspace_ownership_tests
        PRIVATE BetterPullReview::Application glaze::glaze Qt6::Core Threads::Threads
    )
    target_compile_definitions(
        test_better_pull_review_agent_workspace_ownership_tests
        PRIVATE BPR_WORKSPACE_OWNERSHIP_FAULT_INJECTION=1
    )
    if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(
            test_better_pull_review_agent_workspace_ownership_tests
            PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL
        )
    endif()
    bpr_enable_hardening(test_better_pull_review_agent_workspace_ownership_tests)
    bpr_enable_warnings(test_better_pull_review_agent_workspace_ownership_tests)
    bpr_add_network_isolated_test(
        NAME agent_workspace_ownership
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/agent_workspace_ownership_tests.py"
            --executable "$<TARGET_FILE:test_better_pull_review_agent_workspace_ownership_tests>"
    )
    set_tests_properties(agent_workspace_ownership PROPERTIES TIMEOUT 120)
endif()
