bpr_add_executable(
    test_better_pull_review_designated_live_mutation_gate_tests
    "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate.cpp"
    "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate_tests.cpp"
)
target_compile_features(
    test_better_pull_review_designated_live_mutation_gate_tests
    PRIVATE
        cxx_std_23
)
target_link_libraries(
    test_better_pull_review_designated_live_mutation_gate_tests
    PRIVATE
        Threads::Threads
)
bpr_enable_hardening(test_better_pull_review_designated_live_mutation_gate_tests)
bpr_enable_warnings(test_better_pull_review_designated_live_mutation_gate_tests)
bpr_add_network_isolated_test(
    NAME designated_live_mutation_gate
    COMMAND test_better_pull_review_designated_live_mutation_gate_tests
)

set(bpr_designated_live_canary_surface_material "")
foreach(
    bpr_designated_live_canary_surface_file
    IN ITEMS
        designated_live_mutation_canary.cpp
        designated_live_mutation_canary.hpp
        designated_live_mutation_child.cpp
        designated_live_mutation_child.hpp
)
    set(
        bpr_designated_live_canary_surface_path
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/${bpr_designated_live_canary_surface_file}"
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${bpr_designated_live_canary_surface_path}"
    )
    file(
        SHA256
        "${bpr_designated_live_canary_surface_path}"
        bpr_designated_live_canary_surface_file_sha256
    )
    string(
        APPEND
        bpr_designated_live_canary_surface_material
        "${bpr_designated_live_canary_surface_file}=${bpr_designated_live_canary_surface_file_sha256}\n"
    )
endforeach()
string(
    SHA256
    BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256
    "${bpr_designated_live_canary_surface_material}"
)

if(BPR_BUILD_QT)
    bpr_add_executable(
        tool_better_pull_review_designated_live_mutation_runner
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_child.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_runner.cpp"
    )
    target_compile_features(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            cxx_std_23
    )
    target_link_libraries(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            Qt6::Core
            Qt6::Network
    )
    bpr_enable_hardening(tool_better_pull_review_designated_live_mutation_runner)
    bpr_enable_warnings(tool_better_pull_review_designated_live_mutation_runner)

    bpr_add_executable(
        test_better_pull_review_designated_live_mutation_canary
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
    )
    target_compile_features(
        test_better_pull_review_designated_live_mutation_canary
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(test_better_pull_review_designated_live_mutation_canary)
    bpr_enable_warnings(test_better_pull_review_designated_live_mutation_canary)
    target_compile_definitions(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1
            BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256="${BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256}"
    )

    bpr_add_executable(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_child.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_runner_tests.cpp"
    )
    target_compile_features(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            cxx_std_23
    )
    target_compile_definitions(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1
            BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256="${BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256}"
    )
    target_link_libraries(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            Threads::Threads
    )
    bpr_enable_hardening(test_better_pull_review_designated_live_mutation_runner_digest_mismatch)
    bpr_enable_warnings(test_better_pull_review_designated_live_mutation_runner_digest_mismatch)
    bpr_add_network_isolated_test(
        NAME designated_live_mutation_runner_digest_mismatch
        COMMAND test_better_pull_review_designated_live_mutation_runner_digest_mismatch
    )
endif()

bpr_add_designated_live_mutation_test()

bpr_add_network_isolated_test(
    NAME core_dependency_boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
)

bpr_add_network_isolated_test(
    NAME core_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/core_boundary_tests.py"
)

bpr_add_network_isolated_test(
    NAME architecture_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
)

bpr_add_network_isolated_test(
    NAME architecture_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/architecture_boundary_tests.py"
)

bpr_add_network_isolated_test(
    NAME git_protocol_boundary
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/git_protocol_boundary_tests.py"
)

# The branches this host does not compile. Every other gate here is downstream
# of a compiler, and a compiler that never sees `#elif defined(__linux__)`
# cannot report anything about it. Before BPR-0155 three translation units did
# not compile for Windows or Linux while every local gate stayed green.
bpr_add_network_isolated_test(
    NAME conditional_branch_identifiers
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_conditional_branches.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

# The gate above reports on source it cannot compile, so nothing downstream
# catches it being wrong. It read `return _read(x)` as declaring `_read` and
# then, because spelling comparison strips underscores, called the POSIX `read`
# in the opposite branch a rename of it. These cases pin both directions: a real
# rename is still caught, and two correct platform spellings are not.
bpr_add_network_isolated_test(
    NAME conditional_branch_identifier_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/conditional_branch_identifier_tests.py"
)
set_tests_properties(
    conditional_branch_identifiers
    PROPERTIES
        PASS_REGULAR_EXPRESSION "CONDITIONAL_BRANCH_IDENTIFIERS_OK"
        TIMEOUT 60
)

# Only the inventory half runs here. The tests-disabled Release absence scan
# needs a build this graph does not contain, so it stays a closeout command
# documented in docs/development/build-and-test.md.
bpr_add_network_isolated_test(
    NAME rfc0017_test_inventory
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_rfc0017_test_inventory.py"
        --build-root
        "${PROJECT_BINARY_DIR}"
)
set_tests_properties(
    rfc0017_test_inventory
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RFC0017_TEST_INVENTORY_OK"
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME documentation_structure
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_documentation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

bpr_add_network_isolated_test(
    NAME documentation_issue_graph_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_documentation_tests.py"
)

bpr_add_network_isolated_test(
    NAME github_recorded_fixture_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/recorded_pull_request_fixture_tests.py"
)

bpr_add_network_isolated_test(
    NAME attended_review_verdict_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/attended_review_verdict_tests.py"
)

bpr_add_network_isolated_test(
    NAME operator_response_validation_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/.agents/skills/bpr-attended-review/scripts/test_validate_operator_packet.py"
)

bpr_add_network_isolated_test(
    NAME project_source_dump_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/make_project_source_dumps_tests.py"
)

bpr_add_network_isolated_test(
    NAME runtime_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_runtime_surface_tests.py"
)

bpr_add_network_isolated_test(
    NAME binary_dependency_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_binary_dependencies_tests.py"
)

bpr_add_network_isolated_test(
    NAME composed_host_verifier_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_composed_hosts_tests.py"
)
set_tests_properties(
    composed_host_verifier_contract
    PROPERTIES
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME contained_temporary_usage_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/contained_temporary_usage_tests.py"
)

bpr_add_network_isolated_test(
    NAME coding_standard_format_gate_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/coding_standard_format_gate_tests.py"
)

bpr_add_network_isolated_test(
    NAME platform_configuration_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/platform_configuration_tests.py"
)

bpr_add_network_isolated_test(
    NAME application_icon_assets_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/app_icon_assets_tests.py"
)

bpr_add_network_isolated_test(
    NAME dif_assets_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/dif_asset_tests.py"
        --cxx
        "${CMAKE_CXX_COMPILER}"
)

bpr_add_network_isolated_test(
    NAME dependency_modernization_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/dependency_modernization_tests.py"
        --build-dir
        "${CMAKE_BINARY_DIR}"
        --cmake
        "${CMAKE_COMMAND}"
        --cpm-source-cache
        "${CPM_SOURCE_CACHE}"
        --sqlite-source
        "${BPR_SQLITE_SOURCE_DIR}"
)

bpr_add_network_isolated_test(
    NAME github_contract_fixture_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/github_contract_fixture_security_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_device_capture_security
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_device_capture_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_device_live_runner_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_device_live_runner_tests.py"
)

bpr_add_network_isolated_test(
    NAME test_network_isolation_structure_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_test_network_isolation_tests.py"
)

if(APPLE)
    bpr_add_network_isolated_test(
        NAME designated_live_macos_capsule_contract
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/designated_live_macos_capsule_tests.py"
    )
endif()

# BPR-0357. The Mach mechanism BPR-0262 fact_3 and fact_4 rest on exists nowhere
# else in the tree, so it is proven on its own before anything is built on it.
# The target links no product library: this is a platform contract, not a
# composition one, and it must stay outside src/app so no pinned digest moves.
if(APPLE)
    bpr_add_executable(
        test_better_pull_review_macos_service_slot_handshake_tests
        platform/macos_service_slot_handshake_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_macos_service_slot_handshake_tests
        PRIVATE
            "-framework CoreFoundation"
            "-framework Security"
            bsm
    )
    # This target links no product library, so nothing propagates the standard
    # to it the way BetterPullReview::Application does elsewhere.
    target_compile_features(
        test_better_pull_review_macos_service_slot_handshake_tests
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(
        test_better_pull_review_macos_service_slot_handshake_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_macos_service_slot_handshake_tests
    )

    # The linker already applies an ad-hoc signature on arm64, but it sets the
    # linker-signed bit. Re-sign exactly as scripts/designated_live_macos_capsule.py
    # signs the capsule, so the contract answers the question actually asked
    # rather than a friendlier one.
    add_custom_command(
        TARGET test_better_pull_review_macos_service_slot_handshake_tests
        POST_BUILD
        COMMAND
            codesign --force --sign - --timestamp=none
            "$<TARGET_FILE:test_better_pull_review_macos_service_slot_handshake_tests>"
        VERBATIM
        COMMENT "Signing the SERVICE_SLOT contract as the capsule is signed"
    )

    bpr_add_network_isolated_test(
        NAME macos_service_slot_handshake
        COMMAND test_better_pull_review_macos_service_slot_handshake_tests
    )
    set_tests_properties(
        macos_service_slot_handshake
        PROPERTIES
            PASS_REGULAR_EXPRESSION "macos-service-slot-handshake-tests:pass"
    )

    # BPR-0396. The two targets below compile sources out of `src/app/src`, so
    # they belong only to a graph that is allowed to contain application code.
    # `core-only` is not: its whole value is proving the lower tiers stand alone,
    # and a test reaching into `src/app` is part of what that proves. The
    # designated-live capsule these exercise exists only in a Qt application
    # graph in any case, so nothing is lost by scoping them to one.
    if(BPR_BUILD_QT)
        # BPR-0351. The product's own halves of the authenticated launch channel,
        # exercised against each other. This one links the shipped source rather
        # than a copy, so a change to the channel is a change to what is proven.
        bpr_add_executable(
            test_better_pull_review_macos_supervisor_provenance_tests
            platform/macos_supervisor_provenance_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_provenance.cpp"
        )
        target_include_directories(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                "${PROJECT_SOURCE_DIR}/src/app/src"
        )
        target_link_libraries(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                "-framework CoreFoundation"
                "-framework Security"
                bsm
        )
        target_compile_features(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(
            test_better_pull_review_macos_supervisor_provenance_tests
        )
        bpr_enable_warnings(
            test_better_pull_review_macos_supervisor_provenance_tests
        )

        # Signed exactly as the capsule signs, so the contract answers the question
        # actually asked rather than a friendlier one.
        add_custom_command(
            TARGET test_better_pull_review_macos_supervisor_provenance_tests
            POST_BUILD
            COMMAND
                codesign --force --sign - --timestamp=none
                "$<TARGET_FILE:test_better_pull_review_macos_supervisor_provenance_tests>"
            VERBATIM
            COMMENT "Signing the provenance contract as the capsule is signed"
        )

        bpr_add_network_isolated_test(
            NAME macos_supervisor_provenance
            COMMAND test_better_pull_review_macos_supervisor_provenance_tests
        )
        set_tests_properties(
            macos_supervisor_provenance
            PROPERTIES
                PASS_REGULAR_EXPRESSION "macos-supervisor-provenance-tests:pass"
        )

        # BPR-0354. The nested check/use window, closed against a retained
        # descriptor. The barrier is a function call rather than a race.
        bpr_add_executable(
            test_better_pull_review_macos_capsule_identity_tests
            platform/macos_capsule_identity_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_capsule_identity.cpp"
        )
        target_include_directories(
            test_better_pull_review_macos_capsule_identity_tests
            PRIVATE
                "${PROJECT_SOURCE_DIR}/src/app/src"
        )
        target_compile_features(
            test_better_pull_review_macos_capsule_identity_tests
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(test_better_pull_review_macos_capsule_identity_tests)
        bpr_enable_warnings(test_better_pull_review_macos_capsule_identity_tests)
        bpr_add_network_isolated_test(
            NAME macos_capsule_identity
            COMMAND test_better_pull_review_macos_capsule_identity_tests
        )
        set_tests_properties(
            macos_capsule_identity
            PROPERTIES
                PASS_REGULAR_EXPRESSION "macos-capsule-identity-tests:pass"
        )
    endif()
endif()

set(bpr_network_isolation_verifier_command
    Python3::Interpreter
    "${PROJECT_SOURCE_DIR}/scripts/verify_test_network_isolation.py"
    --root
    "${PROJECT_SOURCE_DIR}"
    --build-dir
    "${CMAKE_BINARY_DIR}"
    --ctest
    "${CMAKE_CTEST_COMMAND}"
    --cmake
    "${CMAKE_COMMAND}"
    --python
    "$<TARGET_FILE:Python3::Interpreter>"
    --backend
    "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
)
if(APPLE)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --profile
        "${BPR_MACOS_TEST_SANDBOX_PROFILE}"
        --macos-runner
        "${BPR_MACOS_TEST_SANDBOX_RUNNER}"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --linux-launcher
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
elseif(WIN32)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --windows-launcher
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
endif()
if(BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --live-supervisor
        "${BPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR}"
        --live-inventory
        "${BPR_DESIGNATED_LIVE_MUTATION_INVENTORY}"
        --live-build-dir
        "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        --live-approval
        "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
    )
endif()
bpr_add_network_isolated_test(
    NAME test_network_isolation_manifest
    COMMAND ${bpr_network_isolation_verifier_command}
)
set_tests_properties(
    test_network_isolation_manifest
    PROPERTIES
        TIMEOUT 60
)

foreach(bpr_network_probe IN ITEMS external child)
    bpr_add_network_isolated_test(
        NAME "test_network_isolation_${bpr_network_probe}"
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            "${bpr_network_probe}"
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The launcher's user namespace must buy a network namespace and nothing
    # else. Without this, a future launcher could map the caller to 0 again and
    # every permission assertion in the suite would silently stop testing.
    bpr_add_network_isolated_test(
        NAME test_network_isolation_identity
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            identity
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
    set_tests_properties(
        test_network_isolation_identity
        PROPERTIES
            TIMEOUT 15
    )
endif()
bpr_add_network_isolated_test(
    NAME test_network_isolation_loopback
    LOOPBACK_PORT_COUNT 1
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
        loopback
        "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
)
if(APPLE)
    bpr_add_network_isolated_test(
        NAME test_network_isolation_unrelated
        LOOPBACK_PORT_COUNT 1
        UNRELATED_ENDPOINT_PROBE
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            unrelated
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
endif()
if(TARGET app_better_pull_review)
    bpr_add_network_isolated_test(
        NAME test_network_isolation_application
        COMMAND
            app_better_pull_review
            --network-isolation-probe
    )
endif()
set_tests_properties(
    test_network_isolation_external
    PROPERTIES
        TIMEOUT 15
)
set_tests_properties(
    test_network_isolation_loopback
    PROPERTIES
        TIMEOUT 15
)
if(APPLE)
    set_tests_properties(
        test_network_isolation_unrelated
        PROPERTIES
            TIMEOUT 15
    )
endif()
set_tests_properties(
    test_network_isolation_child
    PROPERTIES
        TIMEOUT 15
)
if(TEST test_network_isolation_application)
    set_tests_properties(
        test_network_isolation_application
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 15
    )
endif()

add_custom_target(
    check_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
    VERBATIM
)

add_subdirectory(core)
add_subdirectory(application)
add_subdirectory(contract)
add_subdirectory(sqlite_contract)

if(BPR_BUILD_QT)
    add_subdirectory(adapters_qt)
    add_subdirectory(github_contract)
    add_subdirectory(git_contract)
    add_subdirectory(syntax_contract)
    add_subdirectory(end_to_end)
endif()

# Deliberately last: every `bpr_add_executable` call in the tree must already
# have run, or the manifest describes a subset of the binaries and the
# isolation guard passes by not looking. The list is smaller under `core-only`
# because the Qt suites above are not added there, which is correct - each
# configuration proves the property for the binaries it actually built.
get_property(bpr_automated_executables GLOBAL PROPERTY BPR_AUTOMATED_EXECUTABLES)
if(NOT bpr_automated_executables)
    message(
        FATAL_ERROR
        "No non-production executables were registered. Either bpr_add_executable "
        "stopped collecting them or this block moved above the test "
        "subdirectories; in both cases live_mutation_isolation would pass "
        "against an empty manifest."
    )
endif()
list(LENGTH bpr_automated_executables bpr_automated_executable_count)
message(
    STATUS
    "bpr: live_mutation_isolation covers ${bpr_automated_executable_count} "
    "non-production executables"
)

# The manifest embeds $<TARGET_FILE:...>, whose value differs per
# configuration. A multi-config generator such as Visual Studio evaluates this
# file once per configuration, so a single fixed path would be written
# repeatedly with different content and generation fails. Keying the path by
# configuration is correct for single-config generators too, where $<CONFIG>
# is simply the one build type.
set(bpr_automated_executable_manifest
    "${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executables.txt"
)
set(bpr_automated_executable_files "")
foreach(bpr_automated_target IN LISTS bpr_automated_executables)
    string(
        APPEND
        bpr_automated_executable_files
        "$<TARGET_FILE:${bpr_automated_target}>\n"
    )
endforeach()
file(
    GENERATE
    OUTPUT "${bpr_automated_executable_manifest}"
    CONTENT "${bpr_automated_executable_files}"
)

# The MSVC linker consumes the COFF symbol table, so `dumpbin /symbols` reports
# only a section summary for a linked image and the binary half of the guard
# would be reading an empty table. The objects that composed each binary do
# carry the table, so record them and let the verifier read those on Windows.
# Written on every platform so the two sides cannot drift apart unnoticed, and
# keyed by binary so the guard keeps its per-binary granularity: the claim is
# that a *non-production* binary carries no live provider, not that the tree
# contains none anywhere.
set(bpr_automated_executable_object_manifest
    "${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executable-objects.txt"
)
set(bpr_automated_executable_objects "")
foreach(bpr_automated_target IN LISTS bpr_automated_executables)
    string(
        APPEND
        bpr_automated_executable_objects
        "$<TARGET_FILE:${bpr_automated_target}>"
        "|"
        "$<JOIN:$<TARGET_OBJECTS:${bpr_automated_target}>,|>"
        "\n"
    )
endforeach()
file(
    GENERATE
    OUTPUT "${bpr_automated_executable_object_manifest}"
    CONTENT "${bpr_automated_executable_objects}"
)

bpr_add_network_isolated_test(
    NAME live_mutation_isolation
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_live_mutation_isolation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
        --manifest
        "${bpr_automated_executable_manifest}"
        --object-manifest
        "${bpr_automated_executable_object_manifest}"
)
set_tests_properties(
    live_mutation_isolation
    PROPERTIES
        TIMEOUT 300
)

bpr_add_network_isolated_test(
    NAME live_mutation_isolation_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_live_mutation_isolation_tests.py"
)
# No PASS_REGULAR_EXPRESSION. This is a `unittest` suite and its exit code is
# already the verdict; setting the property makes CTest ignore that exit code
# and pass on a substring match instead. "OK" is satisfied by any incidental
# `..._OK` marker in captured output, which is how a failing suite reported
# Passed.
set_tests_properties(
    live_mutation_isolation_contract
    PROPERTIES
        TIMEOUT 60
)
