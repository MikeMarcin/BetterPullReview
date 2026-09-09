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

bpr_add_executable(
    test_better_pull_review_sqlite_anonymous_epoch_authority_tests
    anonymous_epoch_authority_tests.cpp
)
target_link_libraries(
    test_better_pull_review_sqlite_anonymous_epoch_authority_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
target_compile_definitions(
    test_better_pull_review_sqlite_anonymous_epoch_authority_tests
    PRIVATE BPR_SQLITE_TESTING=1
)
if(WIN32)
    target_link_libraries(
        test_better_pull_review_sqlite_anonymous_epoch_authority_tests
        PRIVATE advapi32
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_sqlite_anonymous_epoch_authority_tests
)
bpr_enable_warnings(
    test_better_pull_review_sqlite_anonymous_epoch_authority_tests
)

bpr_add_network_isolated_test(
    NAME sqlite_anonymous_epoch_authority
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_sqlite_anonymous_epoch_authority_tests
)
set_tests_properties(
    sqlite_anonymous_epoch_authority
    PROPERTIES
        PASS_REGULAR_EXPRESSION
            "sqlite-anonymous-epoch-authority-tests:pass"
        TIMEOUT 60
)

bpr_add_executable(test_device_binding_storage device_binding_storage_tests.cpp)
target_link_libraries(test_device_binding_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_device_binding_storage)
bpr_enable_warnings(test_device_binding_storage)
bpr_add_network_isolated_test(NAME sqlite_device_binding_storage WINDOWS_UNCONTAINED COMMAND test_device_binding_storage)
set_tests_properties(sqlite_device_binding_storage PROPERTIES TIMEOUT 30 PASS_REGULAR_EXPRESSION "device-binding-storage-tests:pass")

bpr_add_executable(
    test_better_pull_review_review_submission_recovery_storage_tests
    review_submission_recovery_storage_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_submission_recovery_storage_tests
    PRIVATE
        BetterPullReview::AdapterSQLite
        BetterPullReview::SQLite3
)
bpr_enable_hardening(
    test_better_pull_review_review_submission_recovery_storage_tests
)
bpr_enable_warnings(
    test_better_pull_review_review_submission_recovery_storage_tests
)
bpr_add_network_isolated_test(
    NAME sqlite_review_submission_recovery_storage
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_review_submission_recovery_storage_tests
)
set_tests_properties(
    sqlite_review_submission_recovery_storage
    PROPERTIES
        PASS_REGULAR_EXPRESSION "review-submission-recovery-storage-tests:pass"
        TIMEOUT 30
)

bpr_add_executable(test_better_pull_review_profile_registry_storage_tests profile_registry_storage_tests.cpp)
target_link_libraries(test_better_pull_review_profile_registry_storage_tests PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_better_pull_review_profile_registry_storage_tests)
bpr_enable_warnings(test_better_pull_review_profile_registry_storage_tests)
bpr_add_network_isolated_test(NAME sqlite_profile_registry_storage COMMAND test_better_pull_review_profile_registry_storage_tests)
set_tests_properties(sqlite_profile_registry_storage PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_profile_storage_root_tests profile_storage_root_tests.cpp)
target_link_libraries(test_better_pull_review_profile_storage_root_tests PRIVATE BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_better_pull_review_profile_storage_root_tests)
bpr_enable_warnings(test_better_pull_review_profile_storage_root_tests)
bpr_add_network_isolated_test(NAME sqlite_profile_storage_root COMMAND test_better_pull_review_profile_storage_root_tests)
set_tests_properties(sqlite_profile_storage_root PROPERTIES TIMEOUT 60)

bpr_add_executable(test_profile_runtime_lease profile_runtime_lease_tests.cpp)
target_compile_definitions(test_profile_runtime_lease PRIVATE BPR_LEASE_TEST_EXECUTABLE="$<TARGET_FILE:test_profile_runtime_lease>")
target_link_libraries(test_profile_runtime_lease PRIVATE BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_profile_runtime_lease)
bpr_enable_warnings(test_profile_runtime_lease)
bpr_add_network_isolated_test(NAME sqlite_profile_runtime_lease WINDOWS_UNCONTAINED COMMAND test_profile_runtime_lease)
set_tests_properties(sqlite_profile_runtime_lease PROPERTIES TIMEOUT 30)

bpr_add_executable(test_better_pull_review_window_review_restoration_storage_tests window_review_restoration_storage_tests.cpp)
target_link_libraries(test_better_pull_review_window_review_restoration_storage_tests PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_better_pull_review_window_review_restoration_storage_tests)
bpr_enable_warnings(test_better_pull_review_window_review_restoration_storage_tests)
bpr_add_network_isolated_test(NAME sqlite_window_review_restoration_storage COMMAND test_better_pull_review_window_review_restoration_storage_tests)
set_tests_properties(sqlite_window_review_restoration_storage PROPERTIES TIMEOUT 60)
