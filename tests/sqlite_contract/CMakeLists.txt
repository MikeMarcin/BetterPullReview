add_executable(
    better_pull_review_sqlite_storage_contract_tests
    epoch_catalog_storage_tests.cpp
)
target_link_libraries(
    better_pull_review_sqlite_storage_contract_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    better_pull_review_sqlite_storage_contract_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        better_pull_review_sqlite_storage_contract_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(
    better_pull_review_sqlite_storage_contract_tests
)
bpr_enable_warnings(
    better_pull_review_sqlite_storage_contract_tests
)

add_test(
    NAME sqlite_epoch_catalog_storage_contract
    COMMAND better_pull_review_sqlite_storage_contract_tests
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

add_executable(
    better_pull_review_sqlite_behavior_contract_tests
    epoch_catalog_behavior_tests.cpp
)
target_link_libraries(
    better_pull_review_sqlite_behavior_contract_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
if(WIN32)
    target_link_libraries(
        better_pull_review_sqlite_behavior_contract_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(
    better_pull_review_sqlite_behavior_contract_tests
)
bpr_enable_warnings(
    better_pull_review_sqlite_behavior_contract_tests
)

add_test(
    NAME sqlite_epoch_catalog_behavior_contract
    COMMAND better_pull_review_sqlite_behavior_contract_tests
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
