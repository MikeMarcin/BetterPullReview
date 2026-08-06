bpr_add_executable(
    test_better_pull_review_sqlite_storage_contract_tests
    epoch_catalog_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_sqlite_storage_contract_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_sqlite_storage_contract_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_sqlite_storage_contract_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_sqlite_storage_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_sqlite_storage_contract_tests
)

add_test(
    NAME sqlite_epoch_catalog_storage_contract
    COMMAND test_better_pull_review_sqlite_storage_contract_tests
)
set_tests_properties(
    sqlite_epoch_catalog_storage_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "sqlite-epoch-catalog-storage-tests:pass"
        TIMEOUT 30
)
if(WIN32)
    set_property(
        TEST sqlite_epoch_catalog_storage_contract
        APPEND PROPERTY LABELS "BPR-0053;target-native-windows"
    )
endif()

bpr_add_executable(
    test_better_pull_review_sqlite_behavior_contract_tests
    epoch_catalog_behavior_tests.cpp
)
target_link_libraries(
    test_better_pull_review_sqlite_behavior_contract_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_sqlite_behavior_contract_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_sqlite_behavior_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_sqlite_behavior_contract_tests
)

add_test(
    NAME sqlite_epoch_catalog_behavior_contract
    COMMAND test_better_pull_review_sqlite_behavior_contract_tests
)
set_tests_properties(
    sqlite_epoch_catalog_behavior_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "sqlite-epoch-catalog-behavior-tests:pass"
        TIMEOUT 180
)
if(WIN32)
    set_property(
        TEST sqlite_epoch_catalog_behavior_contract
        APPEND PROPERTY LABELS "BPR-0053;target-native-windows"
    )
endif()

bpr_add_executable(
    test_better_pull_review_sqlite_review_ledger_tests
    review_ledger_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_sqlite_review_ledger_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_sqlite_review_ledger_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_sqlite_review_ledger_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(test_better_pull_review_sqlite_review_ledger_tests)
bpr_enable_warnings(test_better_pull_review_sqlite_review_ledger_tests)

add_test(
    NAME sqlite_review_ledger_storage
    COMMAND test_better_pull_review_sqlite_review_ledger_tests
)

bpr_add_executable(
    test_better_pull_review_queue_enrollment_tests
    queue_enrollment_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_queue_enrollment_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_queue_enrollment_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_queue_enrollment_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(test_better_pull_review_queue_enrollment_tests)
bpr_enable_warnings(test_better_pull_review_queue_enrollment_tests)

add_test(
    NAME sqlite_queue_enrollment_storage
    COMMAND test_better_pull_review_queue_enrollment_tests
)
set_tests_properties(
    sqlite_queue_enrollment_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "queue-enrollment-storage-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_workbench_settings_tests
    workbench_settings_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_workbench_settings_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_workbench_settings_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_workbench_settings_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(test_better_pull_review_workbench_settings_tests)
bpr_enable_warnings(test_better_pull_review_workbench_settings_tests)

add_test(
    NAME sqlite_workbench_settings_storage
    COMMAND test_better_pull_review_workbench_settings_tests
)
set_tests_properties(
    sqlite_workbench_settings_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "workbench-settings-storage-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_appearance_settings_tests
    appearance_settings_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_appearance_settings_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_appearance_settings_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_appearance_settings_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(test_better_pull_review_appearance_settings_tests)
bpr_enable_warnings(test_better_pull_review_appearance_settings_tests)

add_test(
    NAME sqlite_appearance_settings_storage
    COMMAND test_better_pull_review_appearance_settings_tests
)
set_tests_properties(
    sqlite_appearance_settings_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "appearance-settings-storage-tests:pass"
        TIMEOUT 60
)
