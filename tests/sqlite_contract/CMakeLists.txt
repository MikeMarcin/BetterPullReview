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
target_compile_definitions(
    test_better_pull_review_sqlite_behavior_contract_tests
    PRIVATE BPR_SQLITE_TESTING=1
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
        # BPR-0905: 122.65 s isolated on Linux and >180 s in two full sweeps.
        # 420 s = 3 * 122.65 s rounded up to the next minute (3.42x quiet).
        # The contended completions remain unmeasured, not claimed below 180 s.
        TIMEOUT 420
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
if(WIN32)
    # [[BPR-1138]]. tests/support/scratch_directory.hpp makes a Windows root
    # with the platform target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_review_mutation_recovery_storage_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
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
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(
        test_better_pull_review_sqlite_review_ledger_tests
        PRIVATE advapi32 BetterPullReview::Platform
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
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(
        test_better_pull_review_queue_enrollment_tests
        PRIVATE advapi32 BetterPullReview::Platform
    )
endif()
bpr_enable_hardening(test_better_pull_review_queue_enrollment_tests)
bpr_enable_warnings(test_better_pull_review_queue_enrollment_tests)

bpr_add_network_isolated_test(
    NAME sqlite_queue_enrollment_storage
    WINDOWS_UNCONTAINED
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
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(
        test_better_pull_review_workbench_settings_tests
        PRIVATE advapi32 BetterPullReview::Platform
    )
endif()
bpr_enable_hardening(test_better_pull_review_workbench_settings_tests)
bpr_enable_warnings(test_better_pull_review_workbench_settings_tests)

bpr_add_network_isolated_test(
    NAME sqlite_workbench_settings_storage
    WINDOWS_UNCONTAINED
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
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(
        test_better_pull_review_appearance_settings_tests
        PRIVATE advapi32 BetterPullReview::Platform
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
if(WIN32)
    # [[BPR-1138]]. tests/support/scratch_directory.hpp makes a Windows root
    # with the platform target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_contained_catalog_probe_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
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

# [[BPR-0813]]. Windows private stores bind SQLite's held handle to the
# admitted name, and open under a root outside the ANSI code page.
# [[BPR-0883]]. Every store factory opens under that root, on every platform,
# and on POSIX every store file is owner-only under umask 0.
bpr_add_executable(test_private_store_open private_store_open_tests.cpp)
target_link_libraries(test_private_store_open PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
target_compile_definitions(test_private_store_open PRIVATE BPR_SQLITE_TESTING=1)
bpr_enable_hardening(test_private_store_open)
if(WIN32)
    # [[BPR-1138]]. tests/support/scratch_directory.hpp makes a Windows root
    # with the platform target's owner-private descriptor.
    target_link_libraries(test_private_store_open PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_private_store_open)
bpr_add_network_isolated_test(NAME sqlite_private_store_open WINDOWS_UNCONTAINED COMMAND test_private_store_open)
set_tests_properties(sqlite_private_store_open PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "private-store-open-tests:pass")

bpr_add_executable(test_device_binding_storage device_binding_storage_tests.cpp)
target_link_libraries(test_device_binding_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
# [[BPR-1111]]. The read classifier's pin for results no contract can inject.
target_compile_definitions(test_device_binding_storage PRIVATE BPR_SQLITE_TESTING=1)
bpr_enable_hardening(test_device_binding_storage)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_device_binding_storage PRIVATE advapi32 BetterPullReview::Platform)
endif()
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
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_profile_registry_storage_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_better_pull_review_profile_registry_storage_tests)
bpr_add_network_isolated_test(NAME sqlite_profile_registry_storage WINDOWS_UNCONTAINED COMMAND test_better_pull_review_profile_registry_storage_tests)
set_tests_properties(sqlite_profile_registry_storage PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_profile_storage_root_tests profile_storage_root_tests.cpp)
target_link_libraries(test_better_pull_review_profile_storage_root_tests PRIVATE BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_better_pull_review_profile_storage_root_tests)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_profile_storage_root_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_better_pull_review_profile_storage_root_tests)
bpr_add_network_isolated_test(NAME sqlite_profile_storage_root WINDOWS_UNCONTAINED COMMAND test_better_pull_review_profile_storage_root_tests)
set_tests_properties(sqlite_profile_storage_root PROPERTIES TIMEOUT 60)

bpr_add_executable(test_profile_runtime_lease profile_runtime_lease_tests.cpp)
target_compile_definitions(test_profile_runtime_lease PRIVATE BPR_LEASE_TEST_EXECUTABLE="$<TARGET_FILE:test_profile_runtime_lease>")
target_link_libraries(test_profile_runtime_lease PRIVATE BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_profile_runtime_lease)
bpr_enable_warnings(test_profile_runtime_lease)
bpr_add_network_isolated_test(NAME sqlite_profile_runtime_lease WINDOWS_UNCONTAINED COMMAND test_profile_runtime_lease)
set_tests_properties(sqlite_profile_runtime_lease PROPERTIES TIMEOUT 30)

bpr_add_executable(test_profile_activation profile_activation_tests.cpp)
target_compile_definitions(test_profile_activation PRIVATE BPR_ACTIVATION_TEST_EXECUTABLE="$<TARGET_FILE:test_profile_activation>")
target_link_libraries(test_profile_activation PRIVATE BetterPullReview::AdapterSQLite)
bpr_enable_hardening(test_profile_activation)
bpr_enable_warnings(test_profile_activation)
# [[BPR-0812]] measured the Windows stub inside the AppContainer on 2026-09-17.
# [[BPR-0979]] gives Windows a real lease-bound named pipe, so the row now walks
# the same canonical private root as sqlite_profile_runtime_lease and is pinned
# uncontained by that shape; not yet measured on the Windows station.
bpr_add_network_isolated_test(NAME sqlite_profile_activation WINDOWS_UNCONTAINED COMMAND test_profile_activation)
set_tests_properties(sqlite_profile_activation PROPERTIES TIMEOUT 30)

bpr_add_executable(test_tour_owner_channel tour_owner_channel_tests.cpp)
target_link_libraries(test_tour_owner_channel PRIVATE BetterPullReview::AdapterSQLite)
if(WIN32)
    # [[BPR-1138]]. scratch_root's Windows branch builds the platform's owner-private descriptor.
    target_link_libraries(test_tour_owner_channel PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_hardening(test_tour_owner_channel)
bpr_enable_warnings(test_tour_owner_channel)
# [[BPR-1061]]. Real native sockets under the same private root shape as sqlite_profile_activation.
# [[BPR-1168]]. Uncontained on Windows again: since BPR-1168 it opens a real
# named pipe under a scratch root, which the container refuses (measured
# 2026-10-02; the pin in verify_test_network_isolation.py names it).
bpr_add_network_isolated_test(NAME sqlite_tour_owner_channel WINDOWS_UNCONTAINED COMMAND test_tour_owner_channel)
set_tests_properties(sqlite_tour_owner_channel PROPERTIES TIMEOUT 60)

bpr_add_executable(test_better_pull_review_window_review_restoration_storage_tests window_review_restoration_storage_tests.cpp)
target_link_libraries(test_better_pull_review_window_review_restoration_storage_tests PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_better_pull_review_window_review_restoration_storage_tests)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_window_review_restoration_storage_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_better_pull_review_window_review_restoration_storage_tests)
bpr_add_network_isolated_test(NAME sqlite_window_review_restoration_storage WINDOWS_UNCONTAINED COMMAND test_better_pull_review_window_review_restoration_storage_tests)
set_tests_properties(sqlite_window_review_restoration_storage PROPERTIES TIMEOUT 60)

bpr_add_executable(test_release_maintenance_files release_maintenance_files_tests.cpp)
target_link_libraries(test_release_maintenance_files PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::Platform)
bpr_enable_hardening(test_release_maintenance_files)
bpr_enable_warnings(test_release_maintenance_files)
bpr_add_network_isolated_test(NAME sqlite_release_maintenance_files WINDOWS_UNCONTAINED COMMAND test_release_maintenance_files)
set_tests_properties(sqlite_release_maintenance_files PROPERTIES TIMEOUT 30)

bpr_add_executable(test_cached_review_overview_storage cached_review_overview_storage_tests.cpp)
target_link_libraries(test_cached_review_overview_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_cached_review_overview_storage)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_cached_review_overview_storage PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_cached_review_overview_storage)
bpr_add_network_isolated_test(NAME sqlite_cached_review_overview WINDOWS_UNCONTAINED COMMAND test_cached_review_overview_storage)
set_tests_properties(sqlite_cached_review_overview PROPERTIES TIMEOUT 60)

bpr_add_executable(test_review_tour_storage review_tour_storage_tests.cpp)
target_link_libraries(test_review_tour_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_tour_storage)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_review_tour_storage PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_tour_storage)
bpr_add_network_isolated_test(NAME sqlite_review_tour_storage WINDOWS_UNCONTAINED COMMAND test_review_tour_storage)

bpr_add_executable(test_review_cycle_storage review_cycle_storage_tests.cpp)
target_link_libraries(test_review_cycle_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3)
bpr_enable_hardening(test_review_cycle_storage)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_review_cycle_storage PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_storage)
bpr_add_network_isolated_test(NAME sqlite_review_cycle_storage WINDOWS_UNCONTAINED COMMAND test_review_cycle_storage)

bpr_add_executable(test_review_cycle_runtime review_cycle_runtime_tests.cpp)
target_link_libraries(test_review_cycle_runtime PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::Application)
bpr_enable_hardening(test_review_cycle_runtime)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_review_cycle_runtime PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_review_cycle_runtime)
bpr_add_network_isolated_test(NAME sqlite_review_cycle_runtime WINDOWS_UNCONTAINED COMMAND test_review_cycle_runtime)

# The link closure of the local runtime, computed from the target graph.
function(bpr_review_cycle_link_closure aTarget aResult)
    set(pending "${aTarget}")
    set(seen "")
    while(pending)
        list(POP_FRONT pending current)
        string(REGEX REPLACE "^\\$<LINK_ONLY:(.+)>$" "\\1" current "${current}")
        if(TARGET "${current}")
            get_target_property(aliased "${current}" ALIASED_TARGET)
            if(aliased)
                set(current "${aliased}")
            endif()
        endif()
        if(current IN_LIST seen)
            continue()
        endif()
        list(APPEND seen "${current}")
        if(TARGET "${current}")
            foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
                get_target_property(links "${current}" ${property})
                if(links)
                    list(APPEND pending ${links})
                endif()
            endforeach()
        endif()
    endwhile()
    set(${aResult} "${seen}" PARENT_SCOPE)
endfunction()
bpr_review_cycle_link_closure(test_review_cycle_runtime bpr_review_cycle_closure)
string(REPLACE ";" "|" bpr_review_cycle_closure_text "${bpr_review_cycle_closure}")
bpr_add_network_isolated_test(
    NAME review_cycle_dependency_closure
    COMMAND
        "${CMAKE_COMMAND}"
        "-DCLOSURE=${bpr_review_cycle_closure_text}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/review_cycle_dependency_closure.cmake"
)

if(BPR_BUILD_AGENT_INTEGRATIONS) # [[BPR-1179]]
bpr_add_executable(test_private_review_storage private_review_storage_tests.cpp)
target_link_libraries(test_private_review_storage PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3 BetterPullReview::Application)
if(WIN32)
    target_link_libraries(test_private_review_storage PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_hardening(test_private_review_storage)
bpr_enable_warnings(test_private_review_storage)
bpr_add_network_isolated_test(NAME sqlite_private_review_storage WINDOWS_UNCONTAINED COMMAND test_private_review_storage)
set_tests_properties(sqlite_private_review_storage PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "private-review-storage-tests:pass")

bpr_add_executable(test_private_review_host private_review_host_tests.cpp)
target_link_libraries(test_private_review_host PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3 BetterPullReview::Application)
bpr_enable_hardening(test_private_review_host)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_private_review_host PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_private_review_host)
bpr_add_network_isolated_test(NAME application_private_review_host WINDOWS_UNCONTAINED COMMAND test_private_review_host)
set_tests_properties(application_private_review_host PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "private-review-host-tests:pass")
endif()

# [[BPR-1082]]. Conventional Comments header edits through the durable comment-draft and tour owners.
bpr_add_executable(test_feedback_draft_store feedback_draft_store_tests.cpp)
target_link_libraries(test_feedback_draft_store PRIVATE BetterPullReview::AdapterSQLite BetterPullReview::SQLite3 BetterPullReview::Application)
bpr_enable_hardening(test_feedback_draft_store)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_feedback_draft_store PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_feedback_draft_store)
bpr_add_network_isolated_test(NAME application_feedback_draft_store WINDOWS_UNCONTAINED COMMAND test_feedback_draft_store)
set_tests_properties(application_feedback_draft_store PROPERTIES PASS_REGULAR_EXPRESSION "FEEDBACK_DRAFT_STORE_OK")
