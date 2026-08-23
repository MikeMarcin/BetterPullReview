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

bpr_add_network_isolated_test(
    NAME sqlite_epoch_catalog_storage_contract
    WINDOWS_UNCONTAINED
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

bpr_add_network_isolated_test(
    NAME sqlite_epoch_catalog_behavior_contract
    WINDOWS_UNCONTAINED
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

bpr_add_executable(
    test_better_pull_review_review_mutation_recovery_storage_tests
    review_mutation_recovery_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_mutation_recovery_storage_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
bpr_enable_hardening(
    test_better_pull_review_review_mutation_recovery_storage_tests
)
bpr_enable_warnings(
    test_better_pull_review_review_mutation_recovery_storage_tests
)
bpr_add_network_isolated_test(
    NAME sqlite_review_mutation_recovery_storage
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_review_mutation_recovery_storage_tests
)
set_tests_properties(
    sqlite_review_mutation_recovery_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "review-mutation-recovery-storage-tests:pass"
        TIMEOUT 30
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

bpr_add_network_isolated_test(
    NAME sqlite_review_ledger_storage
    WINDOWS_UNCONTAINED
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
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

bpr_add_network_isolated_test(
    NAME sqlite_appearance_settings_storage
    COMMAND test_better_pull_review_appearance_settings_tests
)
set_tests_properties(
    sqlite_appearance_settings_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "appearance-settings-storage-tests:pass"
        TIMEOUT 60
)

# The Windows boundary is an AppContainer, and this is the one sqlite_contract
# test that has to run inside it: the claim under test is that the product's
# own owner-private descriptor stays usable by a contained process. It carries
# no `WINDOWS_UNCONTAINED` keyword for that reason. The scratch parent is the
# build tree, which the container is granted at configure time.
if(WIN32)
    bpr_add_executable(
        test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
        appcontainer_private_descriptor_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
        PRIVATE
            BetterPullReview::AdapterSQLite
            advapi32
    )
    target_compile_definitions(
        test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
        PRIVATE BPR_SQLITE_TESTING=1
    )
    bpr_enable_hardening(
        test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
    )

    bpr_add_network_isolated_test(
        NAME sqlite_appcontainer_private_descriptor
        COMMAND
            test_better_pull_review_sqlite_appcontainer_private_descriptor_tests
            "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_tests_properties(
        sqlite_appcontainer_private_descriptor
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "appcontainer-private-descriptor-tests:pass"
            TIMEOUT 30
    )
    set_property(
        TEST sqlite_appcontainer_private_descriptor
        APPEND PROPERTY LABELS "BPR-0360;target-native-windows"
    )
endif()

bpr_add_executable(
    test_better_pull_review_sqlite_storage_root_tests
    storage_root_tests.cpp
)
target_link_libraries(
    test_better_pull_review_sqlite_storage_root_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
)
bpr_enable_hardening(test_better_pull_review_sqlite_storage_root_tests)
bpr_enable_warnings(test_better_pull_review_sqlite_storage_root_tests)

# Opens the catalog through its public path, whose retained ancestor chain
# needs the DOS volume name a lowbox cannot resolve (BPR-0362), so on Windows
# this runs outside the container like the other storage contracts.
bpr_add_network_isolated_test(
    NAME sqlite_storage_root_contract
    WINDOWS_UNCONTAINED
    COMMAND
        test_better_pull_review_sqlite_storage_root_tests
        "${CMAKE_CURRENT_BINARY_DIR}"
)
set_tests_properties(
    sqlite_storage_root_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "storage-root-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_contained_catalog_probe_tests
    contained_catalog_probe_tests.cpp
)
target_link_libraries(
    test_better_pull_review_contained_catalog_probe_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
)
bpr_enable_hardening(test_better_pull_review_contained_catalog_probe_tests)
bpr_enable_warnings(test_better_pull_review_contained_catalog_probe_tests)

# BPR-0427's deliberate contained probe: the two catalog contracts run
# outside the container by BPR-0362's measured pins, so this is the one
# registration that asks the catalog for a typed, bounded answer with the
# ancestor chain as the container presents it.
bpr_add_network_isolated_test(
    NAME contained_catalog_probe
    COMMAND test_better_pull_review_contained_catalog_probe_tests
)
set_tests_properties(
    contained_catalog_probe
    PROPERTIES
        TIMEOUT 60
        PASS_REGULAR_EXPRESSION "CONTAINED_CATALOG_PROBE_OK"
)
